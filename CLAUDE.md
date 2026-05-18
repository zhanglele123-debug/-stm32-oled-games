# CLAUDE.md

本文件为 Claude Code (claude.ai/code) 在此仓库中工作时提供指导。

## 项目概述

STM32F103C8 (Cortex-M3) 裸机嵌入式项目 — 一个 OLED 游戏大厅，在 128x64 I2C OLED 屏幕上包含四款可玩游戏。使用 Keil MDK v5 (uVision) 和 ARM Compiler 5 构建。

## 构建与烧录

- **项目文件**: `Project.uvprojx` — 用 Keil uVision 5 打开，F7 编译，F8 通过 ULINK2/ST-Link 烧录
- **编译器**: ARMCC V5.06 update 7 (`D:\Keil_v5\ARM\ARMCC\Bin`)
- **预处理宏**: `USE_STDPERIPH_DRIVER`
- **头文件路径**: `.\Start`, `.\Library`, `.\User`, `.\System`, `.\Hardware`
- **输出**: `Objects\Project.axf` (ELF), `Objects\Project.hex` (HEX, 64K flash 起始地址 `0x08000000`)
- **清理**: `keilkill.bat` 删除所有中间文件和构建产物
- **芯片**: STM32F103C8 (IRAM 0x20000000/20K, IROM 0x08000000/64K)

没有命令行构建方式，必须通过 Keil IDE 编译。`build_output.txt` 记录了最后一次成功构建的结果。

## 代码架构

### 分层结构（由底向上）

1. **Start/** — CMSIS 核心 + STM32F10x 设备头文件。包含 `startup_stm32f10x_md.s`（向量表 + 复位处理）、`core_cm3.c/h`、`system_stm32f10x.c/h`（时钟初始化到 72MHz）
2. **Library/** — STM32 标准外设库 V3.5.0。所有 `stm32f10x_*.c` 驱动（GPIO、RCC、TIM、I2C、USART、ADC 等）以及 `misc.c`（NVIC/SysTick 辅助函数）
3. **System/** — `Delay.c/h`（us/ms/s 忙等延时）、`Timer.c/h`（TIM1-3 初始化封装）
4. **Hardware/** — 板级驱动（`OLED.c`、`Key.c`、`Serial.c`、`AD.c`）以及四款游戏实现（`plane.c`、`maze.c`、`gomoku.c`、`tetris.c`）
5. **User/** — `main.c`（入口，32 行）、`stm32f10x_it.c`（中断处理 — 均为空）、`stm32f10x_conf.h`（外设头文件包含）

### 共享帧缓冲引擎（`game.c`, 477 行）

`game.c` 是核心渲染引擎。管理一个 1024 字节的帧缓冲（`fb_buf[1024]`），索引方式为 `buf[col * pages + page]`，每个字节存储一列中的 8 个垂直像素。

- **横屏模式** (128x64, 8 pages)：直接输出到 OLED
- **竖屏模式** (64x128, 16 pages)：刷新时软件旋转 90° 输出（飞机大战和俄罗斯方块使用）
- 绘制 API：`fb_draw_pixel` / `fb_clr_pixel`（画点/清除点）、`fb_draw_sprite`（列主序精灵，每字节=8 个垂直像素）、`fb_draw_char` / `fb_draw_string`（8x16 ASCII 字符）、`fb_fill_rect` / `fb_draw_rect`（填充/描边矩形）、`fb_draw_icon`（32x32 程序化游戏图标）
- `fb_draw_pause_overlay()` — 清屏后绘制双线框 "PAUSED" 对话框 + 底部操作提示（K4:GO / K1:END）
- 游戏大厅菜单：3 个固定位置显示 32x32 图标，循环回绕（carousel），当前选中高亮双线框 + 居中名称。K2/K3 导航（带 8 帧冷却防双击），K4 确认进入
- `fb_set_orientation()` 会清空帧缓冲并重新配置尺寸；切换方向时调用此函数

### Flash 高分存储（`game.c` 末尾）

- 使用 STM32 内部 Flash 最后一页（`0x0800FC00`, 1KB）存储最高分，掉电不丢失
- Magic number `0xABCD1234` 校验数据有效性
- 目前存储飞机大战和俄罗斯方块的历史最高分
- `Flash_Load()` 启动时调用一次；`Flash_GetHigh(game_id)` 读取；`Flash_SaveHigh(game_id, score)` 仅在刷新纪录时写入

### 游戏架构

每款游戏导出两个函数：`Xxx_Enter()`（初始化并渲染第一帧）和 `Xxx_Loop()`（逐帧逻辑，返回 1 退出回大厅）。游戏在 `main()` 的死循环中同步轮询 — 无 RTOS，游戏逻辑不使用中断。

- **plane.c** (325 行)：竖屏射击游戏。K1/K2 左右移动，K3 发射子弹，短按 K4 暂停 / 长按 K4（≥15 帧）继续。敌机生成、子弹碰撞、爆炸动画。分数随等级递增难度（4 档速度）。游戏结束显示分数 + 历史最高分 + 新纪录提示。开局 8 帧锁定 K4 防止菜单残留触发误暂停。
- **maze.c** (204 行)：迭代 DFS 迷宫生成（20x9 网格，每格 6x6 像素），显式栈（无递归）。玩家用 K1-K4 方向移动到达出口。K1+K3 同时按住约 750ms 暂停，K4 继续 / K1 退出。
- **gomoku.c** (234 行)：8x8 棋盘，双人本地对弈。K1-K3 方向移动光标。短按 K4（<10 帧）右移光标，长按 K4（≥10 帧）落子。黑子实心方块、白子十字星图案，有明显区分。三连即胜。K1+K3 同时按住约 750ms 暂停。
- **tetris.c** (466 行)：竖屏 10x20 标准俄罗斯方块。7 种方块 + 洗牌袋随机（bag randomizer）。K1 左移 / K2 右移（DAS 连发：首帧立即，8 帧后每 3 帧重复），K3 旋转（带 wall kick），K4 暂停/继续。像素级平滑下落重力，速度随等级递增（L0: 18 帧/格，L9+: 4 帧/格）。消行计分（1/2/3/4 行 = 100/300/500/800），每 10 行升一级。游戏结束显示分数 + 历史最高分 + 新纪录提示。

### 硬件引脚映射

| 引脚  | 功能      | 说明                        |
|-------|-----------|-----------------------------|
| PB8   | OLED SDA  | I2C 数据线（软件模拟, 开漏）  |
| PB9   | OLED SCL  | I2C 时钟线（软件模拟, 开漏）  |
| PB12  | K1        | 上拉输入，按下为低电平        |
| PB13  | K2        | 上拉输入，按下为低电平        |
| PB14  | K3        | 上拉输入，按下为低电平        |
| PB15  | K4        | 上拉输入，按下为低电平        |

### 按键输入（`Key.c`, 84 行）

- `Key_Scan()` — 非阻塞积分式消抖扫描。每键独立计数器，连续 5 帧确认按下/释放后才切换输出（滞后特性）。返回当前按住按键的位掩码。**所有游戏循环均使用此函数**
- `Key_GetPress()` — 非阻塞边沿检测。内部调用 `Key_Scan()` 获取消抖后状态，再异或检测上升沿。返回本帧新按下的按键
- `Key_GetNum()` — 阻塞式读取，等待松手后返回。遗留函数，游戏未使用
- 按键掩码：`KEY1_MASK=0x01`, `KEY2_MASK=0x02`, `KEY3_MASK=0x04`, `KEY4_MASK=0x08`

### 全局时钟

`main.c` 中的 `volatile uint32_t g_tick` 每次主循环迭代加 1。用作各游戏随机数种子的熵源。在 `game.h` 中 `extern` 声明。

## 游戏操作速查

| 操作       | 飞机大战        | 迷宫         | 三子棋           | 俄罗斯方块       |
|------------|-----------------|-------------|------------------|-----------------|
| K1         | 左移            | 上移         | 上移光标          | 左移 (DAS)      |
| K2         | 右移            | 下移         | 下移光标          | 右移 (DAS)      |
| K3         | 发射子弹        | 左移         | 左移光标          | 旋转 (wall kick) |
| K4 (短按)  | 暂停            | 右移         | 右移光标 (<10帧)  | 暂停/继续        |
| K4 (长按)  | 继续游戏 (≥15帧) | —           | 落子 (≥10帧)      | —               |
| K1+K3 按住 | —               | 暂停 (~750ms)| 暂停 (~750ms)     | —               |
| 暂停-K4    | —               | 继续         | 继续              | 继续            |
| 暂停-K1    | 结束游戏        | 退出         | 退出              | 结束游戏         |

## 关键约定

- **无动态内存分配** — 所有状态都是静态/全局变量（裸机限制）
- **C99 模式**（Keil 项目中 `--c99` 选项）
- **`--no-multibyte-chars`** 编译选项 — 避免在字符串中使用宽字符
- **没有单元测试** — 调试通过 Keil 模拟器或真实硬件进行
- **OLED 为 128x64 单色屏**（兼容 SSD1306，I2C 地址 0x78）
- 字库数据在 `OLED_Font.h` 中，以 8x16 ASCII 表 `OLED_F8x16[][]` 形式存储
- 游戏暂停统一使用 `fb_draw_pause_overlay()` — 清屏 + 双线框 "PAUSED" 对话框，暂停时先在首帧保存游戏画面快照再叠加覆盖层

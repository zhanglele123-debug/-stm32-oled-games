/**
 * @file    main.c
 * @brief   主程序入口 - OLED游戏大厅
 *          上电后直接进入游戏大厅，可选择：飞机大战、迷宫、三子棋(五子棋简化版)
 *          STM32F103C8 + OLED 128x64 (I2C)
 *          g_tick: 全局时钟计数器，为各游戏提供随机种子熵源
 */

#include "stm32f10x.h"                  /* STM32标准外设库主头文件 */
#include "Delay.h"                      /* 延时函数库 */
#include "OLED.h"                       /* OLED显示屏驱动 */
#include "Key.h"                        /* 按键驱动(非阻塞式) */
#include "game.h"                       /* 游戏大厅 + 帧缓冲引擎 */

volatile uint32_t g_tick = 0;           /* 全局时钟计数器(每帧+1, 用作随机种子) */

int main(void)
{
    /* 外设初始化 */
    Key_Init();                         /* 初始化4个按键(PB12-PB15, 上拉输入) */
    OLED_Init();                        /* 初始化OLED(I2C, PB8=SCL/PB9=SDA) */
    Flash_Load();                       /* 从Flash加载历史最高分 */
    Game_Init();                        /* 初始化帧缓冲引擎 */
    Game_Enter();                       /* 进入游戏大厅主菜单 */

    /* 主循环 */
    while (1)
    {
        Game_Loop();                    /* 游戏大厅循环(含各游戏子循环) */
        g_tick++;                       /* 全局时钟计数(溢出自动回绕) */
    }
}

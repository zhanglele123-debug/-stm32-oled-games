/**
 * @file    game.c
 * @brief   游戏大厅 + 共享帧缓冲引擎实现
 *          帧缓冲: 1D数组 fb_buf[1024], 支持横屏(128x64)和竖屏(64x128)
 *          横屏刷新: 直接写入OLED; 竖屏刷新: 旋转90°后写入
 *          游戏大厅: 带左右回滚的图标选择界面, K2/K3导航, K4选择
 *          每个游戏独立的*_Enter()和*_Loop()函数, 由大厅统一调度
 */

#include "stm32f10x.h"                  /* STM32标准外设库 */
#include "stm32f10x_flash.h"            /* Flash读写(高分存储) */
#include "OLED.h"                       /* OLED驱动(OLED_SetCursor/WriteData) */
#include "Key.h"                        /* 按键扫描 */
#include "game.h"                       /* 游戏框架头文件 */

extern const uint8_t OLED_F8x16[][16];

/* Shared framebuffer: 1D array indexed as buf[col * pages + page] */
static uint8_t fb_buf[1024];
static uint8_t fb_w, fb_h, fb_pages, fb_orient;

/* ---- Orientation ---- */
void fb_set_orientation(uint8_t orient)
{
    fb_orient = orient;
    if (orient == ORIENT_LANDSCAPE) {
        fb_w = 128; fb_h = 64; fb_pages = 8;
    } else {
        fb_w = 64;  fb_h = 128; fb_pages = 16;
    }
    fb_clear();
}

uint8_t fb_get_width(void)  { return fb_w; }
uint8_t fb_get_height(void) { return fb_h; }

/* ---- Drawing primitives ---- */
void fb_clear(void)
{
    uint16_t i;
    for (i = 0; i < fb_w * fb_pages; i++) fb_buf[i] = 0;
}

void fb_draw_pixel(uint8_t x, uint8_t y)
{
    if (x >= fb_w || y >= fb_h) return;
    fb_buf[x * fb_pages + (y >> 3)] |= (1 << (y & 7));
}

void fb_clr_pixel(uint8_t x, uint8_t y)
{
    if (x >= fb_w || y >= fb_h) return;
    fb_buf[x * fb_pages + (y >> 3)] &= ~(1 << (y & 7));
}

void fb_fill_rect(uint8_t x, uint8_t y, uint8_t w, uint8_t h)
{
    uint8_t cx, cy;
    for (cy = 0; cy < h; cy++)
        for (cx = 0; cx < w; cx++)
            fb_draw_pixel(x + cx, y + cy);
}

void fb_draw_rect(uint8_t x, uint8_t y, uint8_t w, uint8_t h)
{
    uint8_t i;
    for (i = 0; i < w; i++) {
        fb_draw_pixel(x + i, y);
        fb_draw_pixel(x + i, y + h - 1);
    }
    for (i = 0; i < h; i++) {
        fb_draw_pixel(x, y + i);
        fb_draw_pixel(x + w - 1, y + i);
    }
}

void fb_draw_sprite(uint8_t vx, uint8_t vy, const uint8_t *data, uint8_t w, uint8_t h)
{
    uint8_t col;
    uint8_t page = vy >> 3;
    uint8_t shift = vy & 7;

    for (col = 0; col < w; col++) {
        uint8_t cx = vx + col;
        if (cx >= fb_w) continue;
        uint16_t d = (uint16_t)data[col] << shift;
        if (page < fb_pages)
            fb_buf[cx * fb_pages + page] |= (uint8_t)(d & 0xFF);
        if (page + 1 < fb_pages)
            fb_buf[cx * fb_pages + page + 1] |= (uint8_t)(d >> 8);
    }
}

void fb_draw_char(uint8_t vx, uint8_t vy, char c)
{
    uint8_t col, pg_top, pg_bot;
    if (c < ' ' || c > '~') return;
    {
        uint8_t idx = c - ' ';
        pg_top = vy >> 3;
        pg_bot = (vy + 8) >> 3;
        for (col = 0; col < 8; col++) {
            uint8_t cx = vx + col;
            if (cx >= fb_w) continue;
            if (pg_top < fb_pages)
                fb_buf[cx * fb_pages + pg_top] |= OLED_F8x16[idx][col];
            if (pg_bot < fb_pages)
                fb_buf[cx * fb_pages + pg_bot] |= OLED_F8x16[idx][col + 8];
        }
    }
}

void fb_draw_string(uint8_t x, uint8_t line, const char *str)
{
    uint8_t vy = line * 16;
    while (*str) {
        fb_draw_char(x, vy, *str);
        x += 8;
        str++;
    }
}

/* ---- Icon drawing (procedural 32x32 icons) ---- */
static void draw_plane_icon(uint8_t ox, uint8_t oy)
{
    /* Fuselage */
    fb_fill_rect(ox + 14, oy + 6, 4, 20);
    /* Nose cone */
    fb_fill_rect(ox + 13, oy + 0, 6, 8);
    fb_fill_rect(ox + 12, oy + 2, 8, 6);
    /* Wings */
    fb_fill_rect(ox + 0,  oy + 12, 32, 4);
    fb_fill_rect(ox + 8,  oy + 10, 16, 2);
    /* Tail wings */
    fb_fill_rect(ox + 4,  oy + 22, 24, 3);
    /* Cockpit */
    fb_fill_rect(ox + 13, oy + 6, 6, 4);
    fb_clr_pixel(ox + 14, oy + 7);
    fb_clr_pixel(ox + 17, oy + 7);
    /* Engine exhaust */
    fb_draw_pixel(ox + 13, oy + 26);
    fb_draw_pixel(ox + 18, oy + 26);
    fb_fill_rect(ox + 14, oy + 27, 4, 2);
}

static void draw_maze_icon(uint8_t ox, uint8_t oy)
{
    /* Outer wall */
    fb_draw_rect(ox + 2, oy + 2, 28, 28);
    fb_draw_rect(ox + 1, oy + 1, 30, 30);
    /* Entrance */
    fb_fill_rect(ox, oy + 12, 4, 6);
    /* Exit */
    fb_fill_rect(ox + 28, oy + 14, 4, 6);
    /* Internal walls */
    fb_fill_rect(ox + 8,  oy + 2,  2, 10);
    fb_fill_rect(ox + 18, oy + 8,  2, 8);
    fb_fill_rect(ox + 12, oy + 2,  2, 6);
    fb_fill_rect(ox + 22, oy + 2,  2, 8);
    fb_fill_rect(ox + 6,  oy + 12, 10, 2);
    fb_fill_rect(ox + 14, oy + 14, 10, 2);
    fb_fill_rect(ox + 4,  oy + 18, 10, 2);
    fb_fill_rect(ox + 16, oy + 20, 8,  2);
    fb_fill_rect(ox + 4,  oy + 22, 14, 2);
    fb_fill_rect(ox + 8,  oy + 24, 16, 2);
    fb_fill_rect(ox + 10, oy + 18, 2,  8);
    fb_fill_rect(ox + 20, oy + 14, 2,  8);
    /* Path dots */
    fb_draw_pixel(ox + 5, oy + 6);
    fb_draw_pixel(ox + 17, oy + 6);
    fb_draw_pixel(ox + 17, oy + 12);
    fb_draw_pixel(ox + 25, oy + 12);
    fb_draw_pixel(ox + 25, oy + 18);
}

static void draw_gomoku_icon(uint8_t ox, uint8_t oy)
{
    /* Board background */
    fb_draw_rect(ox + 2, oy + 2, 28, 28);
    /* Grid - 5x5 mini board */
    uint8_t i;
    for (i = 0; i < 6; i++) {
        fb_fill_rect(ox + 2, oy + 2 + i * 5, 28, 1);
        fb_fill_rect(ox + 2 + i * 5, oy + 2, 1, 28);
    }
    /* Black stones (filled) */
    fb_fill_rect(ox + 8,  oy + 8,  4, 4);
    fb_fill_rect(ox + 18, oy + 13, 4, 4);
    fb_fill_rect(ox + 13, oy + 18, 4, 4);
    /* White stones (outline + dot) */
    fb_draw_rect(ox + 13, oy + 8,  4, 4);
    fb_draw_pixel(ox + 14, oy + 9);
    fb_draw_rect(ox + 8,  oy + 13, 4, 4);
    fb_draw_pixel(ox + 9,  oy + 14);
    fb_draw_rect(ox + 18, oy + 18, 4, 4);
    fb_draw_pixel(ox + 19, oy + 19);
}

static void draw_tetris_icon(uint8_t ox, uint8_t oy)
{
    /* Board background */
    fb_draw_rect(ox + 2, oy + 2, 28, 28);
    /* Grid lines - 4x4 mini grid */
    uint8_t i;
    for (i = 0; i < 5; i++) {
        fb_fill_rect(ox + 2, oy + 2 + i * 6, 28, 1);
        fb_fill_rect(ox + 2 + i * 6, oy + 2, 1, 28);
    }
    /* T-piece (filled) */
    fb_fill_rect(ox + 10, oy + 8,  4, 4);
    fb_fill_rect(ox + 4,  oy + 14, 16, 4);
    /* L-piece */
    fb_fill_rect(ox + 16, oy + 20, 4, 4);
    fb_fill_rect(ox + 10, oy + 20, 10, 4);
}

void fb_draw_icon(uint8_t x, uint8_t y, uint8_t game_id)
{
    switch (game_id) {
    case GAME_PLANE:  draw_plane_icon(x, y);  break;
    case GAME_MAZE:   draw_maze_icon(x, y);   break;
    case GAME_GOMOKU: draw_gomoku_icon(x, y); break;
    case GAME_TETRIS: draw_tetris_icon(x, y); break;
    }
}

/* ---- Flush ---- */
void fb_flush(void)
{
    uint8_t p, col, bit;
    if (fb_orient == ORIENT_LANDSCAPE) {
        for (p = 0; p < 8; p++) {
            OLED_SetCursor(p, 0);
            for (col = 0; col < 128; col++)
                OLED_WriteData(fb_buf[col * 8 + p]);
        }
    } else {
        for (p = 0; p < 8; p++) {
            OLED_SetCursor(p, 0);
            for (col = 0; col < 128; col++) {
                uint8_t byte_out = 0;
                uint8_t vy = 127 - col;
                uint8_t vy_page = vy >> 3;
                uint8_t vy_mask = 1 << (vy & 7);
                for (bit = 0; bit < 8; bit++) {
                    uint8_t vx = p * 8 + bit;
                    if (vx < fb_w && (fb_buf[vx * fb_pages + vy_page] & vy_mask))
                        byte_out |= (1 << bit);
                }
                OLED_WriteData(byte_out);
            }
        }
    }
}

/* ---- 暂停覆盖层(清屏 + 双线框对话框 + 操作提示) ---- */
void fb_draw_pause_overlay(void)
{
    uint8_t w = fb_get_width();
    uint8_t h = fb_get_height();

    fb_clear();                         /* 清屏, 不保留游戏画面作背景 */

    /* 对话框居中参数 */
    uint8_t dx, dy, dw, dh, tx, ty;
    if (w == 128) {                     /* 横屏 128x64 */
        dw = 72; dh = 32;
        dx = (w - dw) / 2;              /* dx=28 */
        dy = (h - dh) / 2 - 2;          /* dy=14 */
        tx = dx + (dw - 48) / 2;        /* "PAUSED"水平居中: tx=40 */
        ty = dy + (dh - 16) / 2;        /* 文字垂直居中: ty=14+8=22 */
    } else {                            /* 竖屏 64x128 */
        dw = 56; dh = 26;
        dx = (w - dw) / 2;              /* dx=4 */
        dy = (h - dh) / 2;              /* dy=51 */
        tx = dx + (dw - 48) / 2;        /* tx=8 */
        ty = dy + (dh - 16) / 2;        /* ty=56 */
    }

    /* 双线边框 */
    fb_draw_rect(dx, dy, dw, dh);
    fb_draw_rect(dx + 1, dy + 1, dw - 2, dh - 2);

    /* "PAUSED" 居中 */
    {
        const char *ps = "PAUSED";
        uint8_t i;
        for (i = 0; i < 6; i++)
            fb_draw_char(tx + i * 8, ty, ps[i]);
    }

    /* 底部操作提示: K4继续 K1退出 */
    if (w == 128) {
        fb_draw_string(0, 3, "K4:GO");
        fb_draw_string(80, 3, "K1:END");
    } else {
        fb_draw_string(0, 6, "K4:GO");
        fb_draw_string(0, 7, "K1:END");
    }
}

/* ---- Game hub ---- */
#define HUB_VISIBLE  3                  /* 屏幕上同时显示的图标数 */

static void render_hub(uint8_t selected, uint8_t ws)
{
    uint8_t i;
    const char *names[] = {"PLANE", "MAZE", "GOMOKU", "TETRIS"};

    fb_clear();

    /* Title */
    fb_draw_string(8, 0, "== GAME HUB ==");

    /* 循环模式下始终显示双向箭头(游戏数>可见数时) */
    if (GAME_COUNT > HUB_VISIBLE) {
        fb_fill_rect(0, 28, 3, 8);
        fb_draw_pixel(1, 27); fb_draw_pixel(1, 36);
        fb_fill_rect(125, 28, 3, 8);
        fb_draw_pixel(126, 27); fb_draw_pixel(126, 36);
    }

    /* 固定3个图标显示位置, 游戏ID取模实现循环 */
    {
        const uint8_t lx_icon[] = {4, 48, 92};
        for (i = 0; i < HUB_VISIBLE; i++) {
            uint8_t gid = (ws + i) % GAME_COUNT;
            fb_draw_icon(lx_icon[i], 16, gid);
            if (gid == selected) {
                fb_draw_rect(lx_icon[i] - 2, 14, 36, 36);
                fb_draw_rect(lx_icon[i] - 1, 15, 34, 34);
            }
        }
    }

    /* 只显示选中游戏名称(居中, 避免重叠) */
    {
        const char *s = names[selected];
        uint8_t len = 0;
        while (s[len]) len++;
        {
            uint8_t cx = (128 - len * 8) / 2;
            while (*s) { fb_draw_char(cx, 48, *s); cx += 8; s++; }
        }
    }

    fb_flush();
}

/* ---- Public API ---- */
void Game_Init(void)
{
    fb_set_orientation(ORIENT_LANDSCAPE);
    render_hub(0, (0 - 1 + GAME_COUNT) % GAME_COUNT);  /* 选中居中: ws=3 */
}

void Game_Enter(void)
{
    fb_set_orientation(ORIENT_LANDSCAPE);
    render_hub(0, (0 - 1 + GAME_COUNT) % GAME_COUNT);
}

uint8_t Game_Loop(void)
{
    static uint8_t selected = 0;
    static uint8_t ws = 0;              /* 窗口起始(左端图标对应的游戏ID) */
    static uint8_t in_game = 0;
    static uint8_t current_game = 0;
    static uint8_t prev_keys = 0;
    static uint8_t nav_cd = 0;          /* 导航冷却(防双击/抖动重复触发) */

    uint8_t keys = Key_Scan();
    uint8_t press = keys & (keys ^ prev_keys);
    prev_keys = keys;

    if (nav_cd > 0) nav_cd--;           /* 冷却递减 */

    if (!in_game) {
        if ((press & KEY2_MASK) && nav_cd == 0) {
            selected = (selected - 1 + GAME_COUNT) % GAME_COUNT;
            ws = (selected - 1 + GAME_COUNT) % GAME_COUNT;
            render_hub(selected, ws);
            nav_cd = 8;                 /* 8帧冷却(~160ms) */
        }
        if ((press & KEY3_MASK) && nav_cd == 0) {
            selected = (selected + 1) % GAME_COUNT;
            ws = (selected - 1 + GAME_COUNT) % GAME_COUNT;
            render_hub(selected, ws);
            nav_cd = 8;
        }
        if (press & KEY4_MASK) {
            in_game = 1;
            current_game = selected;
            switch (current_game) {
            case GAME_PLANE:  Plane_Enter();  break;
            case GAME_MAZE:   Maze_Enter();   break;
            case GAME_GOMOKU: Gomoku_Enter(); break;
            case GAME_TETRIS: Tetris_Enter(); break;
            }
        }
    } else {
        uint8_t exit = 0;
        switch (current_game) {
        case GAME_PLANE:  exit = Plane_Loop();  break;
        case GAME_MAZE:   exit = Maze_Loop();   break;
        case GAME_GOMOKU: exit = Gomoku_Loop(); break;
        case GAME_TETRIS: exit = Tetris_Loop(); break;
        }
        if (exit) {
            in_game = 0;
            fb_set_orientation(ORIENT_LANDSCAPE);
            render_hub(selected, ws);
        }
    }
    return 0;
}

/* ===== Flash高分存储(掉电不丢失) ===== */
#define FLASH_SAVE_ADDR  0x0800FC00     /* 64KB Flash最后一页(1KB) */
#define FLASH_SAVE_MAGIC 0xABCD1234     /* 数据有效性校验 */

typedef struct {
    uint32_t magic;
    uint16_t plane_high;
    uint16_t tetris_high;
} save_data_t;

static save_data_t save_data;

void Flash_Load(void)
{
    save_data_t *p = (save_data_t *)FLASH_SAVE_ADDR;
    if (p->magic == FLASH_SAVE_MAGIC) {
        save_data = *p;
    } else {
        /* 首次上电, 初始化空数据 */
        save_data.magic = FLASH_SAVE_MAGIC;
        save_data.plane_high = 0;
        save_data.tetris_high = 0;
    }
}

static void flash_write(void)
{
    uint32_t addr = FLASH_SAVE_ADDR;
    uint16_t *src = (uint16_t *)&save_data;
    uint8_t i;
    __disable_irq();                    /* Flash写入时禁中断 */
    FLASH_Unlock();
    FLASH_ErasePage(FLASH_SAVE_ADDR);
    for (i = 0; i < sizeof(save_data_t) / 2; i++) {
        FLASH_ProgramHalfWord(addr, *src);
        addr += 2;
        src++;
    }
    FLASH_Lock();
    __enable_irq();
}

uint16_t Flash_GetHigh(uint8_t game_id)
{
    if (game_id == GAME_PLANE)  return save_data.plane_high;
    if (game_id == GAME_TETRIS) return save_data.tetris_high;
    return 0;
}

void Flash_SaveHigh(uint8_t game_id, uint16_t score)
{
    if (game_id == GAME_PLANE && score > save_data.plane_high) {
        save_data.plane_high = score;
        flash_write();
    }
    if (game_id == GAME_TETRIS && score > save_data.tetris_high) {
        save_data.tetris_high = score;
        flash_write();
    }
}

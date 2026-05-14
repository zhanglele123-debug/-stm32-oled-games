/**
 * @file    gomoku.c
 * @brief   三子棋游戏 - 横屏双人对弈(简化五子棋)
 *          8x8棋盘, 每格6像素(共49px), 三连即胜
 *          黑子: 5x5实心方块  白子: 5x5十字星图案
 *          大棋子显示清晰, 与黑白明显区分
 *          短按K4右移, 长按K4(10帧/250ms)落子
 *          暂停: K1+K3同时按住1秒, 精美覆盖层
 */

#include "stm32f10x.h"
#include "game.h"
#include "Key.h"
#include "gomoku.h"

#define BOARD_SIZE      8               /* 8x8棋盘 */
#define CELL_SZ         6               /* 每格6像素 */
#define BOARD_OX        39              /* 棋盘X偏移(水平居中: (128-49)/2=39) */
#define BOARD_OY        0               /* 棋盘Y偏移(顶部对齐, 底部留line3显示状态) */
#define BOARD_PX        (BOARD_SIZE * CELL_SZ + 1) /* 棋盘像素=49 */

#define STONE_SZ        5               /* 棋子大小(在6px格内居中) */

#define EMPTY           0
#define BLACK           1
#define WHITE           2

#define STATE_PLAYING   0
#define STATE_PAUSED    1
#define STATE_GAMEOVER  2

static uint8_t board[BOARD_SIZE][BOARD_SIZE];
static uint8_t cur_x, cur_y, turn, state, winner;
static uint8_t k4_hold, blink_ctr, pause_hold, pause_snapshot;

static void reset_board(void)
{
    uint8_t y, x;
    for (y = 0; y < BOARD_SIZE; y++)
        for (x = 0; x < BOARD_SIZE; x++)
            board[y][x] = EMPTY;
    cur_x = BOARD_SIZE / 2; cur_y = BOARD_SIZE / 2;
    turn = BLACK; state = STATE_PLAYING; winner = EMPTY;
    k4_hold = 0; blink_ctr = 0; pause_hold = 0; pause_snapshot = 0;
}

/* 三连检测 */
static uint8_t check_win(uint8_t y, uint8_t x)
{
    uint8_t p = board[y][x];
    int8_t d;
    if (p == EMPTY) return 0;

    for (d = 0; d < 4; d++) {
        int8_t dx = (d == 0) ? 1 : (d == 1) ? 0 : 1;
        int8_t dy = (d == 0) ? 0 : (d == 1) ? 1 : (d == 3) ? -1 : 1;
        int8_t cx, cy;
        uint8_t cnt = 1;

        cy = y + dy; cx = x + dx;
        while (cy >= 0 && cy < BOARD_SIZE && cx >= 0 && cx < BOARD_SIZE &&
               board[cy][cx] == p) { cnt++; cy += dy; cx += dx; }
        cy = y - dy; cx = x - dx;
        while (cy >= 0 && cy < BOARD_SIZE && cx >= 0 && cx < BOARD_SIZE &&
               board[cy][cx] == p) { cnt++; cy -= dy; cx -= dx; }
        if (cnt >= 3) return 1;         /* 三连! */
    }
    return 0;
}

/* 绘制5x5黑子(实心方块, 四角削圆效果) */
static void draw_black_stone(uint8_t sx, uint8_t sy)
{
    /* 5x5实心, 削去四角单个像素 → 圆角方块 */
    fb_fill_rect(sx, sy, STONE_SZ, STONE_SZ);
    fb_clr_pixel(sx, sy);               /* 左上角 */
    fb_clr_pixel(sx + STONE_SZ - 1, sy); /* 右上角 */
    fb_clr_pixel(sx, sy + STONE_SZ - 1); /* 左下角 */
    fb_clr_pixel(sx + STONE_SZ - 1, sy + STONE_SZ - 1); /* 右下角 */
}

/* 绘制5x5白子(十字星 + 四角点, 与黑子明显不同) */
static void draw_white_stone(uint8_t sx, uint8_t sy)
{
    uint8_t c = sx + STONE_SZ / 2;       /* 中心X */
    uint8_t r = sy + STONE_SZ / 2;       /* 中心Y */
    /* 十字线 */
    fb_draw_pixel(c, sy);                /* 上 */
    fb_draw_pixel(c, sy + STONE_SZ - 1); /* 下 */
    fb_draw_pixel(sx, r);                /* 左 */
    fb_draw_pixel(sx + STONE_SZ - 1, r); /* 右 */
    /* 中心十字 */
    fb_draw_pixel(c - 1, r);
    fb_draw_pixel(c, r);
    fb_draw_pixel(c + 1, r);
    fb_draw_pixel(c, r - 1);
    fb_draw_pixel(c, r + 1);
    /* 四角 */
    fb_draw_pixel(sx, sy);
    fb_draw_pixel(sx + STONE_SZ - 1, sy);
    fb_draw_pixel(sx, sy + STONE_SZ - 1);
    fb_draw_pixel(sx + STONE_SZ - 1, sy + STONE_SZ - 1);
}

static void render_board_frame(void)
{
    uint8_t y, x;

    fb_clear();

    /* 网格线 */
    for (y = 0; y <= BOARD_SIZE; y++) {
        uint8_t py = BOARD_OY + y * CELL_SZ;
        for (x = 0; x < BOARD_PX; x++)
            fb_draw_pixel(BOARD_OX + x, py);
    }
    for (x = 0; x <= BOARD_SIZE; x++) {
        uint8_t px = BOARD_OX + x * CELL_SZ;
        for (y = 0; y < BOARD_PX; y++)
            fb_draw_pixel(px, BOARD_OY + y);
    }

    /* 棋子(5x5, 在6px格内居中偏移0.5px → 偏移0或1) */
    for (y = 0; y < BOARD_SIZE; y++)
        for (x = 0; x < BOARD_SIZE; x++) {
            uint8_t sx = BOARD_OX + x * CELL_SZ + 1; /* 左偏1px居中 */
            uint8_t sy = BOARD_OY + y * CELL_SZ + 1;
            if (board[y][x] == BLACK)
                draw_black_stone(sx, sy);
            else if (board[y][x] == WHITE)
                draw_white_stone(sx, sy);
        }

    /* 增强光标: 双线粗框 + 中心十字, 闪烁显示(2/3时间) */
    if (state == STATE_PLAYING && (blink_ctr < 12)) {
        uint8_t cx = BOARD_OX + cur_x * CELL_SZ;
        uint8_t cy = BOARD_OY + cur_y * CELL_SZ;
        uint8_t mid_x = cx + CELL_SZ / 2;
        uint8_t mid_y = cy + CELL_SZ / 2;
        /* 双线粗边框 */
        fb_draw_rect(cx, cy, CELL_SZ + 1, CELL_SZ + 1);
        fb_draw_rect(cx + 1, cy + 1, CELL_SZ - 1, CELL_SZ - 1);
        /* 中心十字准星 */
        fb_draw_pixel(mid_x, mid_y - 1);
        fb_draw_pixel(mid_x - 1, mid_y);
        fb_draw_pixel(mid_x, mid_y);
        fb_draw_pixel(mid_x + 1, mid_y);
        fb_draw_pixel(mid_x, mid_y + 1);
    }
    if (++blink_ctr >= 16) blink_ctr = 0;

    /* 底部状态栏(line 3, y=48-63, 紧贴棋盘下方) */
    if (state == STATE_GAMEOVER) {
        if (winner == BLACK)
            fb_draw_string(0, 3, "B WINS! K4:NEW ");
        else
            fb_draw_string(0, 3, "W WINS! K4:NEW ");
    } else {
        if (turn == BLACK)
            fb_draw_string(0, 3, "BLACK K4:MOV PUT");
        else
            fb_draw_string(0, 3, "WHITE K4:MOV PUT");
    }

    fb_flush();
}

/* ===== 公开API ===== */
uint8_t Gomoku_Enter(void)
{
    fb_set_orientation(ORIENT_LANDSCAPE);
    reset_board();
    render_board_frame();
    return 0;
}

uint8_t Gomoku_Loop(void)
{
    static uint8_t prev_k = 0;
    uint8_t keys = Key_Scan();
    uint8_t press = keys & (keys ^ prev_k);
    prev_k = keys;

    /* 暂停: K1+K3同时按住约750ms */
    if ((keys & KEY1_MASK) && (keys & KEY3_MASK)) {
        if (state == STATE_PLAYING) {
            pause_hold++;
            if (pause_hold >= 30) {
                state = STATE_PAUSED; pause_snapshot = 0; pause_hold = 0;
            }
        }
    } else { pause_hold = 0; }

    switch (state) {
    case STATE_PLAYING:
        if (press & KEY1_MASK && cur_y > 0) cur_y--;
        if (press & KEY2_MASK && cur_y < BOARD_SIZE - 1) cur_y++;
        if (press & KEY3_MASK && cur_x > 0) cur_x--;

        /* K4: 短按释放→右移, 长按(10帧/250ms)→落子 */
        if (keys & KEY4_MASK) {
            k4_hold++;
            if (k4_hold == 10 && board[cur_y][cur_x] == EMPTY) {
                board[cur_y][cur_x] = turn;
                if (check_win(cur_y, cur_x)) {
                    state = STATE_GAMEOVER; winner = turn;
                } else {
                    turn = (turn == BLACK) ? WHITE : BLACK;
                }
            }
        } else {
            if (k4_hold > 0 && k4_hold < 10) {
                if (cur_x < BOARD_SIZE - 1) cur_x++;
            }
            k4_hold = 0;
        }
        render_board_frame();
        break;

    case STATE_PAUSED:
        if (!pause_snapshot) { render_board_frame(); pause_snapshot = 1; }
        fb_draw_pause_overlay();
        fb_flush();
        if (press & KEY4_MASK) { state = STATE_PLAYING; pause_snapshot = 0; k4_hold = 0; }
        if (press & KEY1_MASK) return 1;
        break;

    case STATE_GAMEOVER:
        if (press & KEY4_MASK) { reset_board(); render_board_frame(); return 0; }
        if (press & KEY1_MASK) return 1;
        break;
    }
    return 0;
}

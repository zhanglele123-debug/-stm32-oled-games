/**
 * @file    maze.c
 * @brief   迷宫游戏 - 横屏解谜游戏
 *          迭代DFS生成20x9随机迷宫(每格6x6像素, 120x54游戏区域)
 *          使用g_tick作为随机种子, 每次生成的迷宫完全不同
 *          暂停: K1+K3同时按住1秒, 精美覆盖层显示
 */

#include "stm32f10x.h"
#include "game.h"
#include "Key.h"
#include "maze.h"

#define MW              20              /* 迷宫宽度(格) */
#define MH              9               /* 迷宫高度(格) */
#define CS              6               /* 每格像素 */

#define W_TOP           1
#define W_RIGHT         2
#define W_BOTTOM        4
#define W_LEFT          8
#define VISITED         16

#define STATE_PLAYING   0
#define STATE_PAUSED    1
#define STATE_WON       2

static uint8_t maze[MH][MW];
static uint8_t px, py, state;
static uint32_t rng;
static uint8_t pause_hold, pause_snapshot;

static const int8_t dx[4] = {0, 1, 0, -1};
static const int8_t dy[4] = {-1, 0, 1, 0};
static const uint8_t wall_bit[4] = {W_TOP, W_RIGHT, W_BOTTOM, W_LEFT};
static const uint8_t opp_wall[4] = {W_BOTTOM, W_LEFT, W_TOP, W_RIGHT};

/* 迭代DFS显式栈(避免递归栈溢出) */
#define STACK_MAX 256
static int8_t stack_x[STACK_MAX], stack_y[STACK_MAX];
static uint16_t stack_sp;

static uint16_t my_rand(void) {
    rng = rng * 1103515245 + 12345;
    return (rng >> 16) & 0x7FFF;
}

static void gen_maze(void)
{
    uint8_t y, x, d, empty_count = 0;
    rng = 12345 + g_tick;               /* 全局时钟做种子 → 每次迷宫都不同 */

    for (y = 0; y < MH; y++)
        for (x = 0; x < MW; x++)
            maze[y][x] = W_TOP | W_RIGHT | W_BOTTOM | W_LEFT;

    stack_sp = 0;
    stack_x[stack_sp] = 0; stack_y[stack_sp] = 0; stack_sp++;
    maze[0][0] |= VISITED;

    while (stack_sp > 0 && empty_count < 1000) {
        x = stack_x[stack_sp - 1];
        y = stack_y[stack_sp - 1];

        int8_t nx_arr[4], ny_arr[4], nd_arr[4];
        uint8_t ncnt = 0;
        for (d = 0; d < 4; d++) {
            int8_t nx = x + dx[d], ny = y + dy[d];
            if (nx >= 0 && nx < MW && ny >= 0 && ny < MH &&
                !(maze[ny][nx] & VISITED)) {
                nx_arr[ncnt] = nx; ny_arr[ncnt] = ny;
                nd_arr[ncnt] = d; ncnt++;
            }
        }

        if (ncnt > 0) {
            uint8_t idx = my_rand() % ncnt;
            int8_t nx = nx_arr[idx], ny = ny_arr[idx];
            uint8_t dir = nd_arr[idx];
            maze[y][x] &= ~wall_bit[dir];
            maze[ny][nx] &= ~opp_wall[dir];
            maze[ny][nx] |= VISITED;
            stack_x[stack_sp] = nx; stack_y[stack_sp] = ny; stack_sp++;
            empty_count = 0;
        } else {
            stack_sp--;
            empty_count++;
        }
    }

    maze[0][0] &= ~W_TOP;
    maze[MH - 1][MW - 1] &= ~W_BOTTOM;
}

static void render_maze_frame(void)
{
    uint8_t y, x, i, ox = 4, oy = 5;
    fb_clear();

    for (y = 0; y < MH; y++)
        for (x = 0; x < MW; x++) {
            uint8_t cx = ox + x * CS, cy = oy + y * CS;
            if (maze[y][x] & W_TOP)
                for (i = 0; i <= CS; i++) fb_draw_pixel(cx + i, cy);
            if (maze[y][x] & W_LEFT)
                for (i = 0; i <= CS; i++) fb_draw_pixel(cx, cy + i);
            if (maze[y][x] & W_BOTTOM)
                for (i = 0; i <= CS; i++) fb_draw_pixel(cx + i, cy + CS);
            if (maze[y][x] & W_RIGHT)
                for (i = 0; i <= CS; i++) fb_draw_pixel(cx + CS, cy + i);
        }

    /* 出口星标 */
    {
        uint8_t ex = ox + (MW - 1) * CS + CS / 2;
        uint8_t ey = oy + (MH - 1) * CS + CS / 2;
        fb_draw_pixel(ex, ey - 2);
        fb_draw_pixel(ex - 1, ey - 1); fb_draw_pixel(ex, ey - 1);
        fb_draw_pixel(ex + 1, ey - 1);
        fb_draw_pixel(ex - 2, ey); fb_draw_pixel(ex - 1, ey);
        fb_draw_pixel(ex, ey); fb_draw_pixel(ex + 1, ey); fb_draw_pixel(ex + 2, ey);
        fb_draw_pixel(ex - 1, ey + 1); fb_draw_pixel(ex, ey + 1);
        fb_draw_pixel(ex + 1, ey + 1);
        fb_draw_pixel(ex, ey + 2);
    }

    /* 玩家(卡通笑脸) */
    {
        uint8_t plx = ox + px * CS + 1, ply = oy + py * CS + 1;
        fb_fill_rect(plx + 1, ply, 2, 1);
        fb_draw_pixel(plx, ply + 1); fb_draw_pixel(plx + 3, ply + 1);
        fb_fill_rect(plx, ply + 2, 4, 2);
        fb_draw_pixel(plx + 1, ply + 3); fb_draw_pixel(plx + 2, ply + 3);
        fb_clr_pixel(plx + 1, ply + 2); fb_clr_pixel(plx + 2, ply + 2);
    }

    fb_draw_string(0, 4, "FIND THE EXIT!");
    fb_draw_string(0, 5, "K1-4:MOVE");
    fb_flush();
}

static void render_win(void)
{
    fb_clear();
    fb_draw_string(0, 1, " YOU WIN!");
    fb_draw_string(0, 2, " ESCAPED ");
    fb_draw_string(0, 3, " THE MAZE");
    fb_draw_string(0, 5, "[K4]新迷宫");
    fb_draw_string(0, 6, "[K1]退出  ");
    fb_flush();
}

uint8_t Maze_Enter(void)
{
    fb_set_orientation(ORIENT_LANDSCAPE);
    gen_maze();
    px = 0; py = 0; state = STATE_PLAYING;
    pause_hold = 0; pause_snapshot = 0;
    render_maze_frame();
    return 0;
}

uint8_t Maze_Loop(void)
{
    static uint8_t prev_k = 0;
    uint8_t keys = Key_Scan();
    uint8_t press = keys & (keys ^ prev_k);
    prev_k = keys;

    /* 暂停检测: K1+K3同时按住约750ms */
    if ((keys & KEY1_MASK) && (keys & KEY3_MASK)) {
        if (state != STATE_WON) {
            pause_hold++;
            if (pause_hold >= 30 && state == STATE_PLAYING) {
                state = STATE_PAUSED; pause_snapshot = 0; pause_hold = 0;
            }
        }
    } else { pause_hold = 0; }

    switch (state) {
    case STATE_PLAYING:
        if (press & KEY1_MASK && py > 0 && !(maze[py][px] & W_TOP)) py--;
        if (press & KEY2_MASK && py < MH-1 && !(maze[py][px] & W_BOTTOM)) py++;
        if (press & KEY3_MASK && px > 0 && !(maze[py][px] & W_LEFT)) px--;
        if (press & KEY4_MASK && px < MW-1 && !(maze[py][px] & W_RIGHT)) px++;
        render_maze_frame();
        if (px == MW - 1 && py == MH - 1) { state = STATE_WON; render_win(); }
        break;

    case STATE_PAUSED:
        if (!pause_snapshot) { render_maze_frame(); pause_snapshot = 1; }
        fb_draw_pause_overlay();
        fb_flush();
        if (press & KEY4_MASK) { state = STATE_PLAYING; pause_snapshot = 0; }
        if (press & KEY1_MASK) return 1;
        break;

    case STATE_WON:
        if (press & KEY4_MASK) { Maze_Enter(); return 0; }
        if (press & KEY1_MASK) return 1;
        break;
    }
    return 0;
}

/**
 * @file    tetris.c
 * @brief   俄罗斯方块 - 竖屏64x128
 *          10x20棋盘(每格4x4像素), 7种标准方块
 *          K1左移 K2右移 K3旋转 K4软降
 *          K1+K3同时按住暂停
 *          使用全局g_tick作为随机种子
 */

#include "stm32f10x.h"
#include "game.h"
#include "Key.h"
#include "tetris.h"

/* 游戏常量 */
#define BOARD_W     10
#define BOARD_H     20
#define CELL_SZ     4                   /* 每格4x4像素 */
#define BOARD_X     2                   /* 棋盘左上角X */
#define BOARD_Y     16                  /* 棋盘左上角Y(为底部提示留空间) */
#define PREVIEW_X   46                  /* 下一方块预览区X */
#define PREVIEW_Y   28                  /* 下一方块预览区Y */

/* 游戏状态 */
#define STATE_MENU      0
#define STATE_PLAYING   1
#define STATE_PAUSED    2
#define STATE_GAMEOVER  3

/* 7种方块, 每种4个旋转状态, 每状态4行(每行低4位=4列) */
static const uint8_t PIECES[7][4][4] = {
    /* I */ {
        {0x0,0xF,0x0,0x0}, {0x2,0x2,0x2,0x2},
        {0x0,0x0,0xF,0x0}, {0x4,0x4,0x4,0x4}
    },
    /* O */ {
        {0x6,0x6,0x0,0x0}, {0x6,0x6,0x0,0x0},
        {0x6,0x6,0x0,0x0}, {0x6,0x6,0x0,0x0}
    },
    /* T */ {
        {0x2,0x7,0x0,0x0}, {0x2,0x3,0x2,0x0},
        {0x0,0x7,0x2,0x0}, {0x2,0x6,0x2,0x0}
    },
    /* S */ {
        {0x3,0x6,0x0,0x0}, {0x2,0x3,0x1,0x0},
        {0x0,0x6,0x3,0x0}, {0x4,0x6,0x2,0x0}
    },
    /* Z */ {
        {0x6,0x3,0x0,0x0}, {0x1,0x3,0x2,0x0},
        {0x0,0x6,0x3,0x0}, {0x2,0x6,0x4,0x0}
    },
    /* J */ {
        {0x4,0x7,0x0,0x0}, {0x2,0x2,0x6,0x0},
        {0x0,0x7,0x1,0x0}, {0x3,0x2,0x2,0x0}
    },
    /* L */ {
        {0x1,0x7,0x0,0x0}, {0x2,0x2,0x3,0x0},
        {0x0,0x7,0x4,0x0}, {0x6,0x2,0x2,0x0}
    },
};

static uint8_t board[BOARD_H][BOARD_W];
static uint8_t state;
static uint8_t cur_piece, cur_rot, next_piece;
static int8_t cur_x, cur_y;
static uint16_t score;
static uint8_t level;
static uint16_t lines;
static uint32_t rng;
static uint8_t frame;
static uint8_t drop_timer;
static uint8_t pixel_offset;            /* 平滑下落: 0..3像素级偏移 */
static uint8_t pause_snapshot;
static uint8_t piece_bag[7];
static uint8_t bag_idx;

/* 伪随机 */
static uint16_t my_rand(void) {
    rng = rng * 1103515245 + 12345;
    return (rng >> 16) & 0x7FFF;
}

/* 洗牌袋: 7种方块各出现一次再重新洗牌 */
static void refill_bag(void) {
    int8_t i, j; uint8_t t;
    for (i = 0; i < 7; i++) piece_bag[i] = i;
    for (i = 6; i > 0; i--) {
        j = my_rand() % (i + 1);
        t = piece_bag[i]; piece_bag[i] = piece_bag[j]; piece_bag[j] = t;
    }
    bag_idx = 0;
}

static uint8_t get_next_piece(void) {
    if (bag_idx >= 7) refill_bag();
    return piece_bag[bag_idx++];
}

/* 碰撞检测 */
static uint8_t collides(uint8_t p, uint8_t r, int8_t px, int8_t py) {
    uint8_t row, col;
    for (row = 0; row < 4; row++) {
        uint8_t bits = PIECES[p][r][row];
        for (col = 0; col < 4; col++) {
            if (!(bits & (1 << col))) continue;
            {
                int8_t bx = px + col;
                int8_t by = py + row;
                if (bx < 0 || bx >= BOARD_W || by >= BOARD_H) return 1;
                if (by >= 0 && board[by][bx]) return 1;
            }
        }
    }
    return 0;
}

/* 锁定方块到棋盘 */
static void lock_piece(void) {
    uint8_t row, col;
    for (row = 0; row < 4; row++) {
        uint8_t bits = PIECES[cur_piece][cur_rot][row];
        for (col = 0; col < 4; col++) {
            if (!(bits & (1 << col))) continue;
            {
                int8_t by = cur_y + row;
                int8_t bx = cur_x + col;
                if (by >= 0 && by < BOARD_H && bx >= 0 && bx < BOARD_W)
                    board[by][bx] = 1;
            }
        }
    }
}

/* 消行 */
static void clear_lines(void) {
    int8_t r, c, wr;
    uint8_t cleared = 0;
    for (r = BOARD_H - 1; r >= 0; r--) {
        uint8_t full = 1;
        for (c = 0; c < BOARD_W; c++)
            if (!board[r][c]) { full = 0; break; }
        if (!full) continue;
        cleared++;
        for (wr = r; wr > 0; wr--)
            for (c = 0; c < BOARD_W; c++)
                board[wr][c] = board[wr - 1][c];
        for (c = 0; c < BOARD_W; c++) board[0][c] = 0;
        r++;                            /* 重新检查当前行 */
    }
    if (cleared) {
        /* 计分: 1行100, 2行300, 3行500, 4行800 */
        static const uint16_t pts[] = {0, 100, 300, 500, 800};
        score += pts[cleared];
        lines += cleared;
        level = lines / 10;
    }
}

/* 生成新方块 */
static uint8_t spawn_piece(void) {
    cur_piece = next_piece;
    cur_rot = 0;
    cur_x = 3;
    cur_y = -1;                         /* 从棋盘顶部上方出现 */
    next_piece = get_next_piece();
    if (collides(cur_piece, cur_rot, cur_x, cur_y)) return 0;
    return 1;
}

/* 获取当前等级的掉落间隔(帧数) — OLED每帧刷新, ~20fps */
static uint8_t drop_interval(void) {
    if (level >= 9) return 4;
    return 18 - level * 2;             /* L0:18(~0.9s) L1:16 ... L8:4 L9+:4 */
}

/* 绘制棋盘边框(双线) */
static void draw_board_frame(void) {
    uint8_t bw = BOARD_W * CELL_SZ;    /* 40 */
    uint8_t bh = BOARD_H * CELL_SZ;    /* 80 */
    fb_draw_rect(BOARD_X - 1, BOARD_Y - 1, bw + 2, bh + 2);
    fb_draw_rect(BOARD_X - 2, BOARD_Y - 2, bw + 4, bh + 4);
}

/* 绘制一个4x4方格(棋盘坐标 → 屏幕坐标) */
static void draw_cell(uint8_t gx, uint8_t gy) {
    uint8_t sx = BOARD_X + gx * CELL_SZ;
    uint8_t sy = BOARD_Y + gy * CELL_SZ;
    fb_fill_rect(sx, sy, CELL_SZ, CELL_SZ);
}

/* 绘制4x4小方格(用于预览区, 不带网格偏移) */
static void draw_small_cell(uint8_t sx, uint8_t sy) {
    fb_fill_rect(sx, sy, 3, 3);
}

/* 在预览区绘制方块 */
static void render_preview(void) {
    uint8_t row, col;
    /* 预览区(预览框内方块本身即标识, 无需额外标签) */
    fb_draw_rect(PREVIEW_X - 1, PREVIEW_Y - 1, 18, 18); /* 边框 */
    /* 绘制下一方块 */
    for (row = 0; row < 4; row++) {
        uint8_t bits = PIECES[next_piece][0][row];
        for (col = 0; col < 4; col++) {
            if (bits & (1 << col))
                draw_small_cell(PREVIEW_X + 1 + col * 4, PREVIEW_Y + 1 + row * 4);
        }
    }
}

/* 渲染游戏画面 */
static void render_game(void) {
    uint8_t r, c;
    fb_clear();

    /* 分数(参照飞机大战的右侧不留空格式, 竖屏64px刚好8字符=64px) */
    {
        char sbuf[9] = "S:      ";      /* 8字符+null, 8*8=64px填满竖屏宽度 */
        uint16_t n = score;
        int8_t pos = 7;
        if (n == 0) sbuf[pos--] = '0';
        else while (n > 0 && pos >= 2) { sbuf[pos--] = '0' + (n % 10); n /= 10; }
        sbuf[8] = '\0';
        fb_draw_string(0, 0, sbuf);
    }

    /* 棋盘边框 */
    draw_board_frame();

    /* 已锁定的方块 */
    for (r = 0; r < BOARD_H; r++)
        for (c = 0; c < BOARD_W; c++)
            if (board[r][c]) draw_cell(c, r);

    /* 当前活动方块(带像素偏移实现平滑下落) */
    {
        uint8_t row, col;
        for (row = 0; row < 4; row++) {
            uint8_t bits = PIECES[cur_piece][cur_rot][row];
            for (col = 0; col < 4; col++) {
                if (!(bits & (1 << col))) continue;
                {
                    int8_t gy = cur_y + row;
                    int8_t gx = cur_x + col;
                    if (gy >= 0) {
                        uint8_t sx = BOARD_X + gx * CELL_SZ;
                        uint8_t sy = BOARD_Y + gy * CELL_SZ + pixel_offset;
                        fb_fill_rect(sx, sy, CELL_SZ, CELL_SZ);
                    }
                }
            }
        }
    }

    /* 预览 */
    render_preview();

    /* 底部操作提示(棋盘y=16~95, 行6起于y=96, 64px=8字符) */
    fb_draw_string(0, 6, "L R o P");    /* K1左移 K2右移 K3旋转 K4暂停 */
    fb_draw_string(0, 7, "K1K2K3K4");

    fb_flush();
}

/* 菜单 */
static void render_menu(void) {
    fb_clear();
    fb_draw_string(0, 0, " _____  ");
    fb_draw_string(0, 1, "|TETRIS|");
    fb_draw_string(0, 2, "|______|");
    fb_draw_string(0, 4, "[K4]开始");
    fb_draw_string(0, 5, "[K1]退出");
    fb_draw_string(0, 6, "HI:");
    {
        uint8_t i; uint16_t n = Flash_GetHigh(GAME_TETRIS);
        char buf[6] = "     ";
        i = 4;
        if (n == 0) { buf[i] = '0'; }
        else while (n > 0) { buf[i--] = '0' + (n % 10); n /= 10; }
        fb_draw_string(24, 6, buf);
    }
    fb_flush();
}

/* 游戏结束画面(与飞机大战风格一致) */
static void render_gameover(void) {
    char buf[9];
    fb_clear();
    fb_draw_string(0, 0, "  游戏  ");
    fb_draw_string(0, 1, "  结束  ");
    {
        uint8_t i; uint16_t n = score;
        for (i = 0; i < 8; i++) buf[i] = ' ';
        buf[0] = 'S'; buf[1] = 'C'; buf[2] = ':';
        i = 7;
        if (n == 0) buf[i--] = '0';
        else while (n > 0 && i >= 3) { buf[i--] = '0' + (n % 10); n /= 10; }
        fb_draw_string(0, 3, buf);
    }
    {
        uint8_t i; uint16_t n = Flash_GetHigh(GAME_TETRIS);
        for (i = 0; i < 8; i++) buf[i] = ' ';
        buf[0] = 'H'; buf[1] = 'I'; buf[2] = ':';
        i = 7;
        if (n == 0) buf[i--] = '0';
        else while (n > 0 && i >= 3) { buf[i--] = '0' + (n % 10); n /= 10; }
        fb_draw_string(0, 4, buf);
    }
    if (score >= Flash_GetHigh(GAME_TETRIS) && score > 0)
        fb_draw_string(0, 5, " 新纪录!");
    fb_draw_string(0, 6, "[K4]重玩");
    fb_draw_string(0, 7, "[K1]退出");
    fb_flush();
}

/* 初始化/重置游戏 */
static void reset_game(void) {
    uint8_t r, c;
    for (r = 0; r < BOARD_H; r++)
        for (c = 0; c < BOARD_W; c++)
            board[r][c] = 0;
    score = 0;
    level = 0;
    lines = 0;
    frame = 0;
    drop_timer = 0;
    pixel_offset = 0;
    pause_snapshot = 0;
    rng = 12345 + g_tick;
    refill_bag();
    next_piece = get_next_piece();
    spawn_piece();
    state = STATE_PLAYING;
}

/* ===== 公开API ===== */
uint8_t Tetris_Enter(void) {
    fb_set_orientation(ORIENT_PORTRAIT);
    state = STATE_MENU;
    score = 0;
    pause_snapshot = 0;
    rng = 12345 + g_tick;
    render_menu();
    return 0;
}

uint8_t Tetris_Loop(void) {
    uint8_t keys = Key_Scan();

    switch (state) {
    case STATE_MENU:
        if (keys & KEY4_MASK) { reset_game(); render_game(); }
        if (keys & KEY1_MASK) return 1;
        break;

    case STATE_PLAYING: {
        static uint8_t prev_keys = 0;
        static uint8_t k4_prev = 0;         /* K4边沿检测(暂停切换) */
        uint8_t press = keys & (keys ^ prev_keys);
        prev_keys = keys;

        /* K4暂停切换(边沿触发, 按一下暂停/继续) */
        if ((keys & KEY4_MASK) && !k4_prev) {
            state = STATE_PAUSED;
            pause_snapshot = 0;
            k4_prev = 1;
            break;
        }
        k4_prev = (keys & KEY4_MASK) ? 1 : 0;

        /* 左右移动(DAS连发: 首帧立即移动, 8帧后每3帧重复) */
        {
            static uint8_t k1_hold = 0, k2_hold = 0;
            if (keys & KEY1_MASK) {
                if (k1_hold == 0 || (k1_hold >= 8 && ((k1_hold - 8) % 3 == 0))) {
                    if (!collides(cur_piece, cur_rot, cur_x - 1, cur_y))
                        cur_x--;
                }
                k1_hold++;
            } else { k1_hold = 0; }
            if (keys & KEY2_MASK) {
                if (k2_hold == 0 || (k2_hold >= 8 && ((k2_hold - 8) % 3 == 0))) {
                    if (!collides(cur_piece, cur_rot, cur_x + 1, cur_y))
                        cur_x++;
                }
                k2_hold++;
            } else { k2_hold = 0; }
        }

        /* 旋转(边沿触发) */
        if (press & KEY3_MASK) {
            uint8_t new_rot = (cur_rot + 1) % 4;
            if (!collides(cur_piece, new_rot, cur_x, cur_y))
                cur_rot = new_rot;
            else if (!collides(cur_piece, new_rot, cur_x - 1, cur_y))
                { cur_rot = new_rot; cur_x--; }
            else if (!collides(cur_piece, new_rot, cur_x + 1, cur_y))
                { cur_rot = new_rot; cur_x++; }
        }

        /* 平滑重力: 像素级下落, 越界前提前锁定 */
        {
            uint8_t interval = drop_interval();
            uint8_t px_rate = interval / CELL_SZ;
            if (px_rate < 1) px_rate = 1;
            drop_timer++;
            if (drop_timer >= px_rate) {
                drop_timer = 0;
                /* 像素偏移前先检查: 若下一格有碰撞则立即锁定, 防止越界画面 */
                if (pixel_offset == 0 &&
                    collides(cur_piece, cur_rot, cur_x, cur_y + 1)) {
                    lock_piece();
                    clear_lines();
                    if (!spawn_piece()) {
                        state = STATE_GAMEOVER;
                        Flash_SaveHigh(GAME_TETRIS, score);
                        render_gameover();
                        break;
                    }
                } else {
                    pixel_offset++;
                    if (pixel_offset >= CELL_SZ) {
                        pixel_offset = 0;
                        cur_y++;
                    }
                }
            }
        }

        frame++;
        render_game();                      /* 每帧刷新, OLED I2C自然限速≈25fps */
        break;
    }

    case STATE_PAUSED: {
        static uint8_t k4_p_prev = 0;
        if (!pause_snapshot) {
            render_game();
            pause_snapshot = 1;
        }
        fb_draw_pause_overlay();
        fb_flush();
        /* K4继续, K1退出(与五子棋/迷宫一致) */
        if ((keys & KEY4_MASK) && !k4_p_prev) {
            state = STATE_PLAYING;
            pause_snapshot = 0;
        }
        k4_p_prev = (keys & KEY4_MASK) ? 1 : 0;
        if (keys & KEY1_MASK) {
            state = STATE_GAMEOVER;
            Flash_SaveHigh(GAME_TETRIS, score);
            render_gameover();
        }
        break;
    }

    case STATE_GAMEOVER:
        if (keys & KEY4_MASK) { reset_game(); render_game(); }
        if (keys & KEY1_MASK) {
            Flash_SaveHigh(GAME_TETRIS, score);
            state = STATE_MENU; render_menu(); return 1;
        }
        break;
    }
    return 0;
}

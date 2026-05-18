/**
 * @file    plane.c
 * @brief   飞机大战游戏 - 竖屏射击游戏
 *          竖屏64x128渲染(旋转90°输出到128x64物理OLED)
 *          短按K4暂停, 长按K4(≥15帧=375ms)取消暂停
 *          使用全局g_tick作为随机种子, 每次开局敌机位置不同
 *          精美暂停覆盖层: 棋盘格暗化 + 双线框对话框
 */

#include "stm32f10x.h"
#include "game.h"                       /* 共享帧缓冲API */
#include "Key.h"                        /* 按键扫描 */
#include "plane.h"

/* 游戏状态 */
#define STATE_MENU      0               /* 主菜单 */
#define STATE_PLAYING   1               /* 游戏中 */
#define STATE_PAUSED    2               /* 暂停(短按K4进入, 长按K4退出) */
#define STATE_GAMEOVER  3               /* 游戏结束 */

/* 游戏常量 */
#define PLAYER_W        9               /* 玩家飞机宽度 */
#define ENEMY_W         8               /* 敌机宽度 */
#define ENEMY_H         8               /* 敌机高度 */
#define BULLET_W        2               /* 子弹宽度 */
#define BULLET_H        6               /* 子弹高度 */
#define MAX_ENEMIES     5               /* 最大敌机数 */
#define MAX_BULLETS     3               /* 最大子弹数 */
#define MAX_EXPLOSIONS  3               /* 最大爆炸特效数 */
#define PLAYER_SPEED    2               /* 玩家速度 */
#define BULLET_SPEED    3               /* 子弹速度 */
#define GAME_AREA_TOP   16              /* 游戏区顶部 */
#define PLAYER_Y        118             /* 玩家Y坐标 */

typedef struct { int16_t x, y; uint8_t active; } Object;
typedef struct { uint8_t x, y, frame; uint8_t active; } Explosion;

static uint8_t state, frame, fire_cd, player_x;
static uint16_t score, high_score;       /* high_score 从Flash加载, 掉电保存 */
static uint32_t rng;
static Object bullets[MAX_BULLETS];
static Object enemies[MAX_ENEMIES];
static Explosion explosions[MAX_EXPLOSIONS];
static uint8_t pause_snapshot;          /* 暂停快照标志(防闪烁) */
static uint8_t k4_pause_hold;           /* K4按住计数器(长按检测) */
static uint8_t k4_prev_state;           /* K4上一帧状态(边沿检测) */
static uint8_t k4_start_lock;           /* 开局后短暂锁定K4(防止继承按键导致误暂停) */

/* 精灵数据 */
static const uint8_t ship[PLAYER_W] = {
    0x10, 0x38, 0x7C, 0xFE, 0xFF, 0xFE, 0x7C, 0x38, 0x10,
};
static const uint8_t enemy_spr[ENEMY_W] = {
    0x18, 0x3C, 0x7E, 0xDB, 0xFF, 0x5A, 0x24, 0x00,
};
static const uint8_t bullet_spr[BULLET_W] = { 0x38, 0x38 };
static const uint8_t expl_spr[8][2] = {
    {0x18, 0x00}, {0x3C, 0x18}, {0x7E, 0x3C}, {0xFF, 0x7E},
    {0x7E, 0x3C}, {0x3C, 0x18}, {0x18, 0x00}, {0x00, 0x00},
};

/* 伪随机数生成器(种子含g_tick, 每次游戏开局不同) */
static uint16_t my_rand(void) {
    rng = rng * 1103515245 + 12345;
    return (rng >> 16) & 0x7FFF;
}

static void spawn_enemy(void) {
    uint8_t i;
    for (i = 0; i < MAX_ENEMIES; i++)
        if (!enemies[i].active) {
            enemies[i].x = (my_rand() % (fb_get_width() - ENEMY_W - 4)) + 2;
            enemies[i].y = GAME_AREA_TOP;
            enemies[i].active = 1;
            break;
        }
}

static void fire_bullet(void) {
    uint8_t i;
    for (i = 0; i < MAX_BULLETS; i++)
        if (!bullets[i].active) {
            bullets[i].x = player_x + (PLAYER_W / 2) - (BULLET_W / 2);
            bullets[i].y = PLAYER_Y - BULLET_H;
            bullets[i].active = 1;
            break;
        }
}

static void spawn_explosion(uint8_t x, uint8_t y) {
    uint8_t i;
    for (i = 0; i < MAX_EXPLOSIONS; i++)
        if (!explosions[i].active) {
            explosions[i].x = x; explosions[i].y = y;
            explosions[i].frame = 0; explosions[i].active = 1;
            break;
        }
}

static uint8_t hit_test(uint8_t ax, uint8_t ay, uint8_t aw, uint8_t ah,
                         uint8_t bx, uint8_t by, uint8_t bw, uint8_t bh) {
    return (ax + aw > bx && ax < bx + bw && ay + ah > by && ay < by + bh);
}

static void reset(void) {
    uint8_t i;
    score = 0; frame = 0; fire_cd = 0;
    rng = 12345 + g_tick;               /* 用全局时钟做种子 → 每次开局都不同 */
    player_x = (fb_get_width() - PLAYER_W) / 2;
    pause_snapshot = 0;
    k4_pause_hold = 0;
    k4_prev_state = 0;
    k4_start_lock = 8;                  /* 开局锁定K4 8帧(200ms), 防止菜单K4残留触发暂停 */
    for (i = 0; i < MAX_BULLETS; i++) bullets[i].active = 0;
    for (i = 0; i < MAX_ENEMIES; i++) enemies[i].active = 0;
    for (i = 0; i < MAX_EXPLOSIONS; i++) explosions[i].active = 0;
}

static void update(void) {
    uint8_t i, j;
    uint8_t keys = Key_Scan();

    /* 短按K4 → 暂停(开局锁定期间忽略, 防止菜单按键残留) */
    if (k4_start_lock > 0) {
        k4_start_lock--;
    } else {
        if (keys & KEY4_MASK) {
            if (!k4_prev_state) { state = STATE_PAUSED; pause_snapshot = 0; }
        }
        k4_prev_state = (keys & KEY4_MASK) ? 1 : 0;
    }

    if (keys & KEY1_MASK) { if (player_x >= PLAYER_SPEED) player_x -= PLAYER_SPEED; }
    if (keys & KEY2_MASK) {
        if (player_x < fb_get_width() - PLAYER_W - PLAYER_SPEED) player_x += PLAYER_SPEED;
    }
    if (fire_cd > 0) fire_cd--;
    if ((keys & KEY3_MASK) && fire_cd == 0) { fire_bullet(); fire_cd = 4; }

    for (i = 0; i < MAX_BULLETS; i++)
        if (bullets[i].active) {
            if (bullets[i].y >= GAME_AREA_TOP + BULLET_SPEED)
                bullets[i].y -= BULLET_SPEED;
            else bullets[i].active = 0;
        }

    {
        uint8_t spd = (score >= 300) ? 1 : (score >= 200) ? 2 : (score >= 100) ? 3 : 4;
        if ((frame % spd) == 0)
            for (i = 0; i < MAX_ENEMIES; i++)
                if (enemies[i].active) {
                    enemies[i].y++;
                    if (enemies[i].y >= PLAYER_Y - ENEMY_H) {
                        state = STATE_GAMEOVER;
                        if (score > high_score) { high_score = score; Flash_SaveHigh(GAME_PLANE, score); }
                        return;
                    }
                }
    }

    {
        uint8_t sp = (score >= 300) ? 14 : (score >= 200) ? 18 :
                     (score >= 100) ? 24 : (score >= 50) ? 30 : 40;
        if ((frame % sp) == 0) spawn_enemy();
    }

    for (i = 0; i < MAX_BULLETS; i++) {
        if (!bullets[i].active) continue;
        for (j = 0; j < MAX_ENEMIES; j++) {
            if (!enemies[j].active) continue;
            if (hit_test((uint8_t)bullets[i].x, (uint8_t)bullets[i].y, BULLET_W, BULLET_H,
                         (uint8_t)enemies[j].x, (uint8_t)enemies[j].y, ENEMY_W, ENEMY_H)) {
                bullets[i].active = 0;
                spawn_explosion((uint8_t)enemies[j].x, (uint8_t)enemies[j].y);
                enemies[j].active = 0;
                score += 10;
                break;
            }
        }
    }

    for (i = 0; i < MAX_EXPLOSIONS; i++)
        if (explosions[i].active) {
            explosions[i].frame++;
            if (explosions[i].frame >= 8) explosions[i].active = 0;
        }

    frame++;
}

/* 渲染游戏画面(不含fb_flush, 供暂停时合并覆盖层) */
static void render_game_frame(void) {
    uint8_t i;
    char buf[9] = "S:     ";
    uint16_t n = score;
    int8_t pos = 7;
    if (n == 0) buf[pos--] = '0';
    else while (n > 0 && pos >= 2) { buf[pos--] = '0' + (n % 10); n /= 10; }

    fb_clear();
    for (i = 0; i < fb_get_width(); i++) fb_draw_pixel(i, 127);
    for (i = 0; i < MAX_EXPLOSIONS; i++)
        if (explosions[i].active) {
            uint8_t f = explosions[i].frame;
            fb_draw_sprite(explosions[i].x, explosions[i].y,
                          (const uint8_t*)expl_spr[f], 2, 8);
        }
    for (i = 0; i < MAX_ENEMIES; i++)
        if (enemies[i].active)
            fb_draw_sprite((uint8_t)enemies[i].x, (uint8_t)enemies[i].y,
                          enemy_spr, ENEMY_W, ENEMY_H);
    for (i = 0; i < MAX_BULLETS; i++)
        if (bullets[i].active)
            fb_draw_sprite((uint8_t)bullets[i].x, (uint8_t)bullets[i].y,
                          bullet_spr, BULLET_W, BULLET_H);
    fb_draw_sprite(player_x, PLAYER_Y, ship, PLAYER_W, 8);
    fb_draw_string(0, 0, buf);
}

static void render_menu(void) {
    fb_clear();
    fb_draw_string(0, 0, "  ____  ");
    fb_draw_string(0, 1, " /    \\ ");
    fb_draw_string(0, 2, "/PLANE \\");
    fb_draw_string(0, 3, "\\ WAR  /");
    fb_draw_string(0, 4, " \\____/ ");
    fb_draw_string(0, 6, "[K4]START");
    fb_draw_string(0, 7, "[K1]EXIT ");
    fb_flush();
}

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
        uint8_t i; uint16_t n = high_score;
        for (i = 0; i < 8; i++) buf[i] = ' ';
        buf[0] = 'H'; buf[1] = 'I'; buf[2] = ':';
        i = 7;
        if (n == 0) buf[i--] = '0';
        else while (n > 0 && i >= 3) { buf[i--] = '0' + (n % 10); n /= 10; }
        fb_draw_string(0, 4, buf);
    }
    if (score >= high_score && score > 0)
        fb_draw_string(0, 5, " 新纪录!");
    fb_draw_string(0, 6, "[K4]重玩");
    fb_draw_string(0, 7, "[K1]退出");
    fb_flush();
}

/* ===== 公开API ===== */
uint8_t Plane_Enter(void) {
    fb_set_orientation(ORIENT_PORTRAIT);
    state = STATE_MENU;
    score = 0; high_score = Flash_GetHigh(GAME_PLANE);
    pause_snapshot = 0; k4_pause_hold = 0;
    render_menu();
    return 0;
}

uint8_t Plane_Loop(void) {
    uint8_t keys = Key_Scan();

    switch (state) {
    case STATE_MENU:
        if (keys & KEY4_MASK) { reset(); state = STATE_PLAYING; }
        if (keys & KEY1_MASK) return 1;
        break;

    case STATE_PLAYING:
        update();
        if (state == STATE_PLAYING) {
            render_game_frame(); fb_flush();
        } else if (state == STATE_GAMEOVER) {
            render_gameover();
        }
        break;

    case STATE_PAUSED:
        /* 首帧渲染游戏快照, 后续帧仅叠加覆盖层(防闪烁) */
        if (!pause_snapshot) {
            render_game_frame();
            pause_snapshot = 1;
        }
        fb_draw_pause_overlay();        /* 精美暂停覆盖层 */
        fb_flush();                     /* 单次刷新 */
        /* 长按K4 (≥15帧=375ms) → 取消暂停 */
        if (keys & KEY4_MASK) {
            k4_pause_hold++;
            if (k4_pause_hold >= 15) {
                state = STATE_PLAYING;
                pause_snapshot = 0;
                k4_pause_hold = 0;
            }
        } else {
            k4_pause_hold = 0;
        }
        if (keys & KEY1_MASK) {         /* K1: 退出游戏 */
            state = STATE_GAMEOVER;
            if (score > high_score) { high_score = score; Flash_SaveHigh(GAME_PLANE, score); }
            render_gameover();
        }
        break;

    case STATE_GAMEOVER:
        if (keys & KEY4_MASK) { reset(); state = STATE_PLAYING; }
        if (keys & KEY1_MASK) {
            if (score > high_score) { high_score = score; Flash_SaveHigh(GAME_PLANE, score); }
            state = STATE_MENU; render_menu(); return 1;
        }
        break;
    }
    return 0;
}

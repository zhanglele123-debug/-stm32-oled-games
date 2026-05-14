/**
 * @file    game.h
 * @brief   游戏大厅框架 - 共享帧缓冲引擎 + 游戏选择菜单
 *          支持横屏(128x64)和竖屏(64x128旋转90°)两种渲染模式
 *          提供统一的像素/精灵/字符绘制API和帧缓冲刷新接口
 *          管理3款游戏: 飞机大战、迷宫、三子棋
 */

#ifndef __GAME_H
#define __GAME_H

#include "stm32f10x.h"

/* 全局时钟计数器(外部声明, 定义在main.c) */
extern volatile uint32_t g_tick;

/* 屏幕方向 */
#define ORIENT_LANDSCAPE  0             /* 横屏128x64(迷宫/三子棋) */
#define ORIENT_PORTRAIT   1             /* 竖屏64x128(飞机大战, 旋转90°输出) */

/* 游戏ID */
#define GAME_PLANE   0                  /* 飞机大战 */
#define GAME_MAZE    1                  /* 迷宫 */
#define GAME_GOMOKU  2                  /* 三子棋 */
#define GAME_COUNT   3                  /* 游戏总数 */

/* ===== 帧缓冲API ===== */
void fb_set_orientation(uint8_t orient); /* 设置屏幕方向(自动清屏) */
void fb_clear(void);                     /* 清空帧缓冲 */
void fb_draw_sprite(uint8_t x, uint8_t y, const uint8_t *data,
                    uint8_t w, uint8_t h); /* 绘制精灵(列主序, 每字节8垂直像素) */
void fb_draw_char(uint8_t x, uint8_t y, char c); /* 绘制8x16 ASCII字符 */
void fb_draw_string(uint8_t x, uint8_t line, const char *str); /* 绘制字符串 */
void fb_draw_pixel(uint8_t x, uint8_t y);   /* 画点 */
void fb_clr_pixel(uint8_t x, uint8_t y);    /* 清除点 */
void fb_fill_rect(uint8_t x, uint8_t y, uint8_t w, uint8_t h); /* 填充矩形 */
void fb_draw_rect(uint8_t x, uint8_t y, uint8_t w, uint8_t h); /* 描边矩形 */
void fb_flush(void);                     /* 刷新到OLED(横屏直接输出, 竖屏旋转90°) */
uint8_t fb_get_width(void);              /* 获取当前方向宽度 */
uint8_t fb_get_height(void);             /* 获取当前方向高度 */
void fb_draw_icon(uint8_t x, uint8_t y, uint8_t game_id); /* 绘制32x32游戏图标 */

/* 精美暂停覆盖层(棋盘格暗化 + 双线框对话框) */
void fb_draw_pause_overlay(void);

/* ===== 游戏大厅 ===== */
void Game_Init(void);                    /* 初始化游戏大厅 */
void Game_Enter(void);                   /* 进入游戏大厅主菜单 */
uint8_t Game_Loop(void);                 /* 游戏大厅主循环(含子游戏循环) */

/* ===== 子游戏入口(在各游戏.c中实现) ===== */
uint8_t Plane_Enter(void);               /* 进入飞机大战 */
uint8_t Plane_Loop(void);                /* 飞机大战循环 */
uint8_t Maze_Enter(void);                /* 进入迷宫 */
uint8_t Maze_Loop(void);                 /* 迷宫循环 */
uint8_t Gomoku_Enter(void);              /* 进入三子棋 */
uint8_t Gomoku_Loop(void);               /* 三子棋循环 */

#endif

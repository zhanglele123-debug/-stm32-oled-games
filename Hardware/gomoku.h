/**
 * @file    gomoku.h
 * @brief   五子棋游戏 - 横屏双人对弈游戏
 *          15x15标准棋盘，黑白双方轮流落子
 *          短按K4移动光标，长按K4落子
 *          检测五连、四方向（横竖斜）判断胜负
 */

#ifndef __GOMOKU_H
#define __GOMOKU_H

#include "stm32f10x.h"

uint8_t Gomoku_Enter(void);             /* 进入五子棋（初始化棋盘） */
uint8_t Gomoku_Loop(void);              /* 五子棋主循环，返回1表示退出 */

#endif

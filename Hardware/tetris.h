/**
 * @file    tetris.h
 * @brief   俄罗斯方块游戏 - 竖屏模式
 *          10x20标准棋盘, 7种标准方块, 分数/等级系统
 */

#ifndef __TETRIS_H
#define __TETRIS_H

#include "stm32f10x.h"

uint8_t Tetris_Enter(void);
uint8_t Tetris_Loop(void);

#endif

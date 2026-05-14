/**
 * @file    maze.h
 * @brief   迷宫游戏 - 横屏解谜游戏
 *          使用递归回溯算法随机生成迷宫
 *          玩家操控角色从左上角走到右下角出口即获胜
 */

#ifndef __MAZE_H
#define __MAZE_H

#include "stm32f10x.h"

uint8_t Maze_Enter(void);               /* 进入迷宫（生成新迷宫并渲染） */
uint8_t Maze_Loop(void);                /* 迷宫主循环，返回1表示退出 */

#endif

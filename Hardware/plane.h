/**
 * @file    plane.h
 * @brief   飞机大战游戏 - 竖屏射击游戏
 *          玩家操控战斗机躲避并消灭从上方来袭的敌机
 *          支持暂停、爆炸动画、难度递增、最高分记录
 */

#ifndef __PLANE_H
#define __PLANE_H

#include "stm32f10x.h"

uint8_t Plane_Enter(void);              /* 进入飞机大战（初始化并显示菜单） */
uint8_t Plane_Loop(void);               /* 飞机大战主循环，返回1表示退出 */

#endif

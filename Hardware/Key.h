/**
 * @file    Key.h
 * @brief   按键驱动 - 4个按键的非阻塞式扫描
 *          PB12=K1, PB13=K2, PB14=K3, PB15=K4 (上拉输入, 按下为低电平)
 *          提供电平扫描(Key_Scan)和边沿检测(Key_GetPress)两种方式
 */

#ifndef __KEY_H
#define __KEY_H

#include "stm32f10x.h"

/* 按键掩码 */
#define KEY1_MASK  0x01                 /* K1 (PB12) */
#define KEY2_MASK  0x02                 /* K2 (PB13) */
#define KEY3_MASK  0x04                 /* K3 (PB14) */
#define KEY4_MASK  0x08                 /* K4 (PB15) */

void Key_Init(void);                    /* 初始化按键GPIO(上拉输入) */
uint8_t Key_GetNum(void);               /* 阻塞式获取键码(等待松手, 兼容旧代码) */
uint8_t Key_Scan(void);                 /* 非阻塞电平扫描(立即返回当前按键状态) */
uint8_t Key_GetPress(void);             /* 非阻塞边沿检测(返回本帧新按下的按键) */

#endif

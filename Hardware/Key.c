/**
 * @file    Key.c
 * @brief   按键驱动实现(积分式非阻塞消抖)
 *          Key_Scan(): 返回消抖后的按键掩码(3帧确认)
 *          Key_GetPress(): 边沿检测(基于消抖后的状态)
 *          Key_GetNum(): 阻塞式读取(保留兼容, 游戏不使用)
 */

#include "stm32f10x.h"                  /* STM32标准外设库 */
#include "Key.h"                        /* 按键头文件 */

#define DEBOUNCE_THRESHOLD  5           /* 按下/释放需连续5帧确认(抗强抖动) */

static uint8_t db_state = 0;            /* 消抖稳定输出 */
static uint8_t db_ctr[4];               /* 每键积分计数器(0~3) */
static uint8_t key_prev_state = 0;      /* 上一帧状态(边沿检测用) */

/* 初始化按键GPIO: PB12-PB15 上拉输入 */
void Key_Init(void)
{
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);

    GPIO_InitTypeDef GPIO_InitStructure;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_12 | GPIO_Pin_13 |
                                  GPIO_Pin_14 | GPIO_Pin_15;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOB, &GPIO_InitStructure);
}

/* 阻塞式获取键码(等待按下并松手, 保留兼容旧代码) */
uint8_t Key_GetNum(void)
{
    uint8_t KeyNum = 0;
    /* 使用消抖后的状态判断按下, 轮询等待松手 */
    while (KeyNum == 0) {
        uint8_t s = Key_Scan();
        if (s & KEY1_MASK) { while (Key_Scan() & KEY1_MASK); KeyNum = 1; }
        if (s & KEY2_MASK) { while (Key_Scan() & KEY2_MASK); KeyNum = 2; }
        if (s & KEY3_MASK) { while (Key_Scan() & KEY3_MASK); KeyNum = 3; }
        if (s & KEY4_MASK) { while (Key_Scan() & KEY4_MASK); KeyNum = 4; }
    }
    return KeyNum;
}

/* 非阻塞电平扫描: 积分消抖, 3帧确认后返回稳定状态 */
uint8_t Key_Scan(void)
{
    uint8_t raw = 0;
    uint8_t i;

    /* 读取原始GPIO(按下=低电平→置1) */
    if (GPIO_ReadInputDataBit(GPIOB, GPIO_Pin_12) == 0) raw |= KEY1_MASK;
    if (GPIO_ReadInputDataBit(GPIOB, GPIO_Pin_13) == 0) raw |= KEY2_MASK;
    if (GPIO_ReadInputDataBit(GPIOB, GPIO_Pin_14) == 0) raw |= KEY3_MASK;
    if (GPIO_ReadInputDataBit(GPIOB, GPIO_Pin_15) == 0) raw |= KEY4_MASK;

    /* 积分消抖: 每键独立计数, 达到阈值才切换输出(滞后特性) */
    for (i = 0; i < 4; i++) {
        if (raw & (1 << i)) {
            if (db_ctr[i] < DEBOUNCE_THRESHOLD) {
                db_ctr[i]++;
                if (db_ctr[i] >= DEBOUNCE_THRESHOLD)
                    db_state |= (1 << i);           /* 按下确认 */
            }
        } else {
            if (db_ctr[i] > 0) {
                db_ctr[i]--;
                if (db_ctr[i] == 0)
                    db_state &= ~(1 << i);          /* 释放确认 */
            }
        }
    }
    return db_state;
}

/* 非阻塞边沿检测: 返回本帧新按下的按键(基于消抖后状态) */
uint8_t Key_GetPress(void)
{
    uint8_t current = Key_Scan();
    uint8_t press = current & (current ^ key_prev_state);
    key_prev_state = current;
    return press;
}

/**
 * @file    Key.c
 * @brief   按键驱动实现
 *          Key_Scan(): 非阻塞电平扫描, 立即返回当前按下的按键掩码
 *          Key_GetPress(): 非阻塞边沿检测, 返回本帧新按下的按键(上升沿)
 *          Key_GetNum(): 阻塞式读取(保留兼容旧代码, 游戏不使用)
 */

#include "stm32f10x.h"                  /* STM32标准外设库 */
#include "Delay.h"                      /* 延时函数(消抖用) */
#include "Key.h"                        /* 按键头文件 */

static uint8_t key_prev_state = 0;      /* 上一帧按键状态(边沿检测用) */

/* 初始化按键GPIO: PB12-PB15 上拉输入 */
void Key_Init(void)
{
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);  /* 开启GPIOB时钟 */

    GPIO_InitTypeDef GPIO_InitStructure;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;          /* 上拉输入 */
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_12 | GPIO_Pin_13 |
                                  GPIO_Pin_14 | GPIO_Pin_15;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOB, &GPIO_InitStructure);
}

/* 阻塞式获取键码(等待按键按下并松手后才返回) */
uint8_t Key_GetNum(void)
{
    uint8_t KeyNum = 0;

    if (GPIO_ReadInputDataBit(GPIOB, GPIO_Pin_12) == 0)
    {
        Delay_ms(20);                                           /* 消抖 */
        while (GPIO_ReadInputDataBit(GPIOB, GPIO_Pin_12) == 0); /* 等待松手 */
        Delay_ms(20);
        KeyNum = 1;
    }
    if (GPIO_ReadInputDataBit(GPIOB, GPIO_Pin_13) == 0)
    {
        Delay_ms(20);
        while (GPIO_ReadInputDataBit(GPIOB, GPIO_Pin_13) == 0);
        Delay_ms(20);
        KeyNum = 2;
    }
    if (GPIO_ReadInputDataBit(GPIOB, GPIO_Pin_14) == 0)
    {
        Delay_ms(20);
        while (GPIO_ReadInputDataBit(GPIOB, GPIO_Pin_14) == 0);
        Delay_ms(20);
        KeyNum = 3;
    }
    if (GPIO_ReadInputDataBit(GPIOB, GPIO_Pin_15) == 0)
    {
        Delay_ms(20);
        while (GPIO_ReadInputDataBit(GPIOB, GPIO_Pin_15) == 0);
        Delay_ms(20);
        KeyNum = 4;
    }
    return KeyNum;                      /* 0=无按键, 1-4=对应按键 */
}

/* 非阻塞电平扫描: 立即返回当前按下的按键位掩码 */
uint8_t Key_Scan(void)
{
    uint8_t current = 0;
    /* 按下为低电平, 读为0 → 置对应位 */
    if (GPIO_ReadInputDataBit(GPIOB, GPIO_Pin_12) == 0) current |= KEY1_MASK;
    if (GPIO_ReadInputDataBit(GPIOB, GPIO_Pin_13) == 0) current |= KEY2_MASK;
    if (GPIO_ReadInputDataBit(GPIOB, GPIO_Pin_14) == 0) current |= KEY3_MASK;
    if (GPIO_ReadInputDataBit(GPIOB, GPIO_Pin_15) == 0) current |= KEY4_MASK;
    return current;
}

/* 非阻塞边沿检测: 返回本帧新按下的按键(0→1上升沿) */
uint8_t Key_GetPress(void)
{
    uint8_t current = Key_Scan();               /* 读取当前状态 */
    uint8_t press = current & (current ^ key_prev_state); /* 上升沿 = 当前为1 且 上帧为0 */
    key_prev_state = current;                   /* 更新上一帧状态 */
    return press;
}

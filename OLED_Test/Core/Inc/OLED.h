#ifndef __OLED_H
#define __OLED_H

#include "stm32f1xx_hal.h"

// OLED I2C 地址（常见 0x3C）
#define OLED_ADDR       (0x3C << 1)  // HAL 库要求左移 1 位

// 屏幕尺寸
#define OLED_WIDTH      128
#define OLED_HEIGHT     64

// 函数声明
void OLED_Init(void);
void OLED_Clear(void);
void OLED_ShowChar(uint8_t x, uint8_t y, char ch);
void OLED_ShowString(uint8_t x, uint8_t y, char *str);
void OLED_ShowBuffer(uint8_t *buf);

#endif

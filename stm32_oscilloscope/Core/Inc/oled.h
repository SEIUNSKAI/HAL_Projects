#ifndef __OLED_H
#define __OLED_H

#include "stm32f1xx_hal.h"

#define OLED_WIDTH 128
#define OLED_HEIGHT 64
#define OLED_ADDR (0x3C<<1)

void OLED_Init(void);
void OLED_Clear(void);
void OLED_DrawPixel(uint8_t x,uint8_t y);
void OLED_Refresh(void);

#endif
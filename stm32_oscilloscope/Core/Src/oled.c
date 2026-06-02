#include "oled.h"
#include <string.h>

extern I2C_HandleTypeDef hi2c1;

static uint8_t oled_buffer[OLED_WIDTH * OLED_HEIGHT / 8];

static void OLED_WriteCommand(uint8_t cmd)
{
    uint8_t data[2];
    data[0] = 0x00;
    data[1] = cmd;
    HAL_I2C_Master_Transmit(&hi2c1, OLED_ADDR, data, 2, HAL_MAX_DELAY);
}

static void OLED_WriteData(uint8_t* data, uint16_t size)
{
    if (size > 128) return;
    uint8_t buf[129];
    buf[0] = 0x40;
    memcpy(&buf[1], data, size);
    HAL_I2C_Master_Transmit(&hi2c1, OLED_ADDR, buf, size + 1, HAL_MAX_DELAY);
}

void OLED_Init(void)
{
    HAL_Delay(100);

    OLED_WriteCommand(0xAE);
    OLED_WriteCommand(0x20);
    OLED_WriteCommand(0x10);
    OLED_WriteCommand(0xB0);
    OLED_WriteCommand(0xC8);
    OLED_WriteCommand(0x00);
    OLED_WriteCommand(0x10);
    OLED_WriteCommand(0x40);
    OLED_WriteCommand(0x81);
    OLED_WriteCommand(0xFF);
    OLED_WriteCommand(0xA1);
    OLED_WriteCommand(0xA6);
    OLED_WriteCommand(0xA8);
    OLED_WriteCommand(0x3F);
    OLED_WriteCommand(0xA4);
    OLED_WriteCommand(0xD3);
    OLED_WriteCommand(0x00);
    OLED_WriteCommand(0xD5);
    OLED_WriteCommand(0xF0);
    OLED_WriteCommand(0xD9);
    OLED_WriteCommand(0x22);
    OLED_WriteCommand(0xDA);
    OLED_WriteCommand(0x12);
    OLED_WriteCommand(0xDB);
    OLED_WriteCommand(0x20);
    OLED_WriteCommand(0x8D);
    OLED_WriteCommand(0x14);
    OLED_WriteCommand(0xAF);

    OLED_Clear();
    OLED_Refresh();
}

void OLED_Clear(void)
{
    memset(oled_buffer,0,sizeof(oled_buffer));
}

void OLED_DrawPixel(uint8_t x,uint8_t y)
{
    if(x>=OLED_WIDTH || y>=OLED_HEIGHT) return;

    oled_buffer[x + (y/8)*OLED_WIDTH] |= 1<<(y%8);
}

void OLED_Refresh(void)
{
    for (uint8_t page = 0; page < OLED_HEIGHT / 8; page++)
    {
        OLED_WriteCommand(0xB0+page);
        OLED_WriteCommand(0x00);
        OLED_WriteCommand(0x10);

        OLED_WriteData(&oled_buffer[OLED_WIDTH*page],OLED_WIDTH);
    }
}

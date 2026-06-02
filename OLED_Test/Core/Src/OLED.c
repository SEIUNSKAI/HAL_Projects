#include "oled.h"
#include "stm32f1xx_hal.h"
#include "fonts.h"  

extern I2C_HandleTypeDef hi2c1;

// 复位引脚（如果有，可选）
#define OLED_RES_GPIO_Port GPIOB
#define OLED_RES_Pin       GPIO_PIN_0

// 内部函数：发送命令或数据
static void OLED_SendCommand(uint8_t cmd)
{
    uint8_t buf[2] = {0x00, cmd}; // 控制字 0x00 表示命令
    HAL_I2C_Master_Transmit(&hi2c1, OLED_ADDR, buf, 2, 500);
}

static void OLED_SendData(uint8_t data)
{
    uint8_t buf[2] = {0x40, data}; // 控制字 0x40 表示数据
    HAL_I2C_Master_Transmit(&hi2c1, OLED_ADDR, buf, 2, 500);
}

// 延时复位 OLED
static void OLED_Reset(void)
{
#ifdef OLED_RES_GPIO_Port
    HAL_GPIO_WritePin(OLED_RES_GPIO_Port, OLED_RES_Pin, GPIO_PIN_RESET);
    HAL_Delay(50);
    HAL_GPIO_WritePin(OLED_RES_GPIO_Port, OLED_RES_Pin, GPIO_PIN_SET);
    HAL_Delay(50);
#endif
}

// 初始化 OLED
void OLED_Init(void)
{
    OLED_Reset();

    HAL_Delay(100);

    OLED_SendCommand(0xAE); // Display OFF
    OLED_SendCommand(0xD5); // Set display clock div
    OLED_SendCommand(0x80); // Suggested ratio
    OLED_SendCommand(0xA8); // Set multiplex
    OLED_SendCommand(0x3F); // 64 rows
    OLED_SendCommand(0xD3); // Set display offset
    OLED_SendCommand(0x00); // No offset
    OLED_SendCommand(0x40); // Set start line = 0
    OLED_SendCommand(0x8D); // Charge pump
    OLED_SendCommand(0x14); // Enable
    OLED_SendCommand(0x20); // Memory mode
    OLED_SendCommand(0x00); // Horizontal addressing
    OLED_SendCommand(0xA1); // Segment remap
    OLED_SendCommand(0xC8); // COM scan dec
    OLED_SendCommand(0xDA); // COM pins
    OLED_SendCommand(0x12);
    OLED_SendCommand(0x81); // Contrast
    OLED_SendCommand(0xCF);
    OLED_SendCommand(0xD9); // Pre-charge
    OLED_SendCommand(0xF1);
    OLED_SendCommand(0xDB); // VCOM detect
    OLED_SendCommand(0x40);
    OLED_SendCommand(0xA4); // Resume RAM content
    OLED_SendCommand(0xA6); // Normal display
    OLED_SendCommand(0xAF); // Display ON

    OLED_Clear();
}

// 清屏
void OLED_Clear(void)
{
    for(uint8_t page = 0; page < 8; page++)
    {
        OLED_SendCommand(0xB0 + page); // 页地址
        OLED_SendCommand(0x00);        // 列低地址
        OLED_SendCommand(0x10);        // 列高地址
        for(uint8_t col = 0; col < OLED_WIDTH; col++)
            OLED_SendData(0x00);
    }
}

// 显示单字符（8x8点阵）
void OLED_ShowChar(uint8_t x, uint8_t y, char ch)
{
    uint8_t i;
    if(ch < 32 || ch > 127) ch = '?'; // 超出字符用 ?
    for(i = 0; i < 8; i++)
    {
        OLED_SendData(Font8x8[ch-32][i]);
    }
}

// 显示字符串
void OLED_ShowString(uint8_t x, uint8_t y, char *str)
{
    uint8_t i = 0;
    while(str[i] != '\0')
    {
        OLED_ShowChar(x, y, str[i]);
        x += 8;
        if(x >= OLED_WIDTH)
        {
            x = 0;
            y++;
        }
        i++;
    }
}

// 显示整块缓冲区（128x64/8 = 1024 bytes）
void OLED_ShowBuffer(uint8_t *buf)
{
    for(uint8_t page = 0; page < 8; page++)
    {
        OLED_SendCommand(0xB0 + page);
        OLED_SendCommand(0x00);
        OLED_SendCommand(0x10);
        for(uint8_t col = 0; col < OLED_WIDTH; col++)
        {
            OLED_SendData(buf[page*128 + col]);
        }
    }
}
#include "oscilloscope.h"
#include "oled.h"
#include "main.h"
#include <stdint.h>

extern ADC_HandleTypeDef hadc1;

#define SAMPLE_POINTS 128
static volatile uint16_t adc_buffer[SAMPLE_POINTS];
static volatile uint8_t sample_index = 0;

// 初始化采样数组
void Oscilloscope_Init(void)
{
    sample_index = 0;
    for(uint8_t i = 0; i < SAMPLE_POINTS; i++)
        adc_buffer[i] = 0;
}

// TIM2中断采样
void Oscilloscope_ADC_Collect(void)
{
    HAL_ADC_Start(&hadc1);
    if (HAL_ADC_PollForConversion(&hadc1, 1) == HAL_OK)
    {
        uint16_t val = HAL_ADC_GetValue(&hadc1);
        adc_buffer[sample_index++] = val;
        if (sample_index >= SAMPLE_POINTS)
            sample_index = 0;
    }
}

// ADC 映射到 OLED Y 坐标
static uint8_t map_adc_to_y(uint16_t adc)
{
    if(adc > 4095) adc = 4095;
    return 63 - ((adc * 64) / 4096); // 0~4095 -> 0~63
}

// 绘制波形
void Oscilloscope_Draw(void)
{
    OLED_Clear();  // 也可以只在主循环开始或每 N 帧清一次
    for(uint8_t i = 0; i < SAMPLE_POINTS; i++)
    {
        uint8_t index = (sample_index + i) % SAMPLE_POINTS;
        uint8_t y = map_adc_to_y(adc_buffer[index]);
        OLED_DrawPixel(i, y);
    }
    OLED_Refresh();
}
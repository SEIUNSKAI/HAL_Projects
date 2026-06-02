/*
 * signal_sampler.c —— 信号采集模块
 *
 * 功能：
 *   使用 TIM3 以 600kHz 触发 ADC1（PA0），DMA 搬运 1024 个采样点到内存，
 *   然后去除直流偏置，输出以 0 为中心的真实交流信号供 DFT 分析。
 *
 * 硬件链路：
 *   PA0 (模拟输入) → ADC1 → DMA2_Stream0 → adc_buffer[1024]
 *   TIM3 TRGO (600kHz) → ADC1 外部触发
 */

#include "signal_sampler.h"

/* 引用 main.c 中的 HAL 外设句柄 */
extern ADC_HandleTypeDef hadc1;
extern TIM_HandleTypeDef htim3;

/* DMA 循环写入的原始 ADC 缓冲区（uint16_t，12bit 右对齐） */
static uint16_t adc_buffer[FFT_SIZE];

/* DMA 传输完成标志，在 ADC 转换完成回调中置位 */
static volatile uint8_t capture_done = 0;

/*
 * ADC 转换完成回调（由 HAL 中断服务调用）
 * DMA 将 1024 个半字全部搬完后触发此回调，置位完成标志
 */
void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *hadc)
{
    if (hadc->Instance == ADC1) {
        capture_done = 1;
    }
}

/*
 * 初始化采样模块
 * 启动 TIM3，开始输出 600kHz 的 TRGO 脉冲作为 ADC 触发源
 *   采样率 = TIM3_CLK / (PSC+1) / (ARR+1) = 84MHz / 1 / 140 = 600kHz
 */
void Sampler_Init(void)
{
    HAL_TIM_Base_Start(&htim3);
}

/*
 * 执行一次信号采集 + 去直流处理（阻塞调用）
 *
 * 流程：
 *   1. 启动 ADC + DMA（NORMAL 模式，传输 1024 个半字）
 *   2. 轮询等待 DMA 传输完成（capture_done 标志）
 *   3. 停止 ADC + DMA
 *   4. 第一遍遍历：ADC 原始值 → float，同时累加求均值
 *   5. 第二遍遍历：每个样本减去均值，消除直流偏置
 *
 * 去直流原理：
 *   原始信号被抬升（约 1.65V DC 偏置），ADC 采样值在 ~2048 附近摆动。
 *   减去均值后信号以 0 为中心，振幅反映真实的正弦波幅度，
 *   这样 DFT 计算才不会被直流分量干扰。
 */
void Sampler_Capture(float *out)
{
    /* --- 启动 ADC + DMA --- */
    capture_done = 0;
    HAL_ADC_Start_DMA(&hadc1, (uint32_t *)adc_buffer, FFT_SIZE);

    /* --- 阻塞等待 DMA 传完 1024 个点 --- */
    while (!capture_done);

    /* --- 关闭 ADC + DMA，准备下一轮 --- */
    HAL_ADC_Stop_DMA(&hadc1);

    /* --- 第一遍：转 float，累加求直流均值 --- */
    float sum = 0.0f;
    for (int i = 0; i < FFT_SIZE; i++) {
        out[i] = (float)adc_buffer[i];
        sum += out[i];
    }
    float mean = sum / (float)FFT_SIZE;

    /* --- 第二遍：减去直流偏置，得到以 0 为中心的交流信号 --- */
    for (int i = 0; i < FFT_SIZE; i++) {
        out[i] -= mean;
    }
}

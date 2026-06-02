#ifndef __SIGNAL_SAMPLER_H
#define __SIGNAL_SAMPLER_H

#include "main.h"

/* ==================== 采样参数 ==================== */
#define FFT_SIZE    1024        /* 采样点数 */
#define SAMPLE_RATE 600000.0f   /* 采样频率 600kHz，由 TIM3 触发 */

/* ==================== 函数声明 ==================== */

/*
 * 初始化采样模块：启动 TIM3，开始以 600kHz 产生 ADC 触发信号
 */
void Sampler_Init(void);

/*
 * 执行一次完整的信号采集：
 *   1. 启动 ADC + DMA，阻塞等待 1024 个采样点完成
 *   2. 将原始 ADC 值转为浮点数，计算并减去直流偏置
 *   3. 将去直流后的数据写入 out[] 数组（调用者提供缓冲区）
 *   out - 输出数组指针，长度至少为 FFT_SIZE
 */
void Sampler_Capture(float *out);

#endif

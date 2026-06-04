/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "OLED.h"
#include "signal_sampler.h"
#include "ad9833.h"
#include <math.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
ADC_HandleTypeDef hadc1;
DMA_HandleTypeDef hdma_adc1;

I2C_HandleTypeDef hi2c1;

SPI_HandleTypeDef hspi1;

TIM_HandleTypeDef htim3;
TIM_HandleTypeDef htim7;

/* USER CODE BEGIN PV */
#define NUM_FREQS 57   /* 20k~300kHz，步长 5kHz */

static float    samples[FFT_SIZE];
static float    dft_results[NUM_FREQS];
static const uint32_t target_freqs[NUM_FREQS] = {
    20000,  25000,  30000,  35000,  40000,  45000,  50000,
    55000,  60000,  65000,  70000,  75000,  80000,  85000,
    90000,  95000, 100000, 105000, 110000, 115000, 120000,
   125000, 130000, 135000, 140000, 145000, 150000, 155000,
   160000, 165000, 170000, 175000, 180000, 185000, 190000,
   195000, 200000, 205000, 210000, 215000, 220000, 225000,
   230000, 235000, 240000, 245000, 250000, 255000, 260000,
   265000, 270000, 275000, 280000, 285000, 290000, 295000,
   300000
};
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_DMA_Init(void);
static void MX_I2C1_Init(void);
static void MX_ADC1_Init(void);
static void MX_TIM3_Init(void);
static void MX_TIM7_Init(void);
static void MX_SPI1_Init(void);
static void MX_DAC_Init(void);
/* USER CODE BEGIN PFP */
/* Goertzel 算法：计算信号 x 在频率 freq 处的 DFT 幅度 */
static float goertzel(float *x, int N, float freq, float fs);
/* 冒泡排序找出幅度最大的两个频率，并保证 fa < fb */
static void find_top_two(float *mags, const uint32_t *freqs, int N,
                         uint32_t *fa, uint32_t *fb);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_DMA_Init();
  MX_I2C1_Init();
  MX_ADC1_Init();
  MX_TIM3_Init();
  MX_TIM7_Init();
  MX_SPI1_Init();
  MX_DAC_Init();
  /* USER CODE BEGIN 2 */

  /* ---- 初始化 OLED ---- */
  OLED_Init(&hi2c1);

  /* ---- 先画固定标签（只画一次，之后只刷新数字） ---- */
  OLED_Clear();
  OLED_ShowString(0, 0,  "fa =");
  OLED_ShowString(80, 0, "kHz");
  OLED_ShowString(0, 16, "fb =");
  OLED_ShowString(80, 16, "kHz");
  OLED_Display();

  /* ---- 启动采样模块（TIM3 + ADC + DMA） ---- */
  Sampler_Init();

  /* ---- 初始化两路 AD9833 DDS 模块（CS1=PB0, CS2=PB1, SPI1） ---- */
  AD9833_Init();

  uint32_t fa = 0, fb = 0;
  uint32_t last_fa = 0xFFFFFFFF, last_fb = 0xFFFFFFFF;  /* 上次显示的值，用于判断是否变化 */

  /* 时序一致性过滤：连续 N 帧检测到同一对频率才算有效 */
  uint32_t candidate_fa = 0, candidate_fb = 0;
  int      confirm_cnt   = 0;
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */

    /* ---- 第一步：采集 1024 点并去直流 ---- */
    Sampler_Capture(samples);

    /* ---- 第二步：对 9 个目标频率依次进行 Goertzel DFT ---- */
    for (int i = 0; i < NUM_FREQS; i++) {
        dft_results[i] = goertzel(samples, FFT_SIZE,
                                  (float)target_freqs[i], SAMPLE_RATE);
    }

    /* ---- 第三步：冒泡排序，找出幅度最大的两个频率 ---- */
    find_top_two(dft_results, target_freqs, NUM_FREQS, &fa, &fb);

    /* ---- 第四步：时序一致性过滤，滤除模拟域互调产物的瞬态干扰 ---- */
    if (fa == candidate_fa && fb == candidate_fb) {
        confirm_cnt++;
        if (confirm_cnt >= 3) {        /* 连续 3 帧相同 → 确认有效 */
            if (fa != last_fa || fb != last_fb) {
                last_fa = fa;
                last_fb = fb;
                OLED_ShowNum(48, 0,  fa / 1000, 3);
                OLED_ShowNum(48, 16, fb / 1000, 3);
                OLED_Display();

                /* 同步更新两路 AD9833 输出频率 */
                AD9833_SetFreq(1, fa);
                AD9833_SetFreq(2, fb);
            }
        }
    } else {
        candidate_fa = fa;
        candidate_fb = fb;
        confirm_cnt  = 1;              /* 新候选，重新计数 */
    }
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
  */
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = 8;
  RCC_OscInitStruct.PLL.PLLN = 336;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 4;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief ADC1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_ADC1_Init(void)
{

  /* USER CODE BEGIN ADC1_Init 0 */

  /* USER CODE END ADC1_Init 0 */

  ADC_ChannelConfTypeDef sConfig = {0};

  /* USER CODE BEGIN ADC1_Init 1 */

  /* USER CODE END ADC1_Init 1 */

  /** Configure the global features of the ADC (Clock, Resolution, Data Alignment and number of conversion)
  */
  hadc1.Instance = ADC1;
  hadc1.Init.ClockPrescaler = ADC_CLOCK_SYNC_PCLK_DIV4;
  hadc1.Init.Resolution = ADC_RESOLUTION_12B;
  hadc1.Init.ScanConvMode = DISABLE;
  hadc1.Init.ContinuousConvMode = DISABLE;
  hadc1.Init.DiscontinuousConvMode = DISABLE;
  hadc1.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_RISING;
  hadc1.Init.ExternalTrigConv = ADC_EXTERNALTRIGCONV_T3_TRGO;
  hadc1.Init.DataAlign = ADC_DATAALIGN_RIGHT;
  hadc1.Init.NbrOfConversion = 1;
  hadc1.Init.DMAContinuousRequests = DISABLE;
  hadc1.Init.EOCSelection = ADC_EOC_SINGLE_CONV;
  if (HAL_ADC_Init(&hadc1) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure for the selected ADC regular channel its corresponding rank in the sequencer and its sample time.
  */
  sConfig.Channel = ADC_CHANNEL_0;
  sConfig.Rank = 1;
  sConfig.SamplingTime = ADC_SAMPLETIME_15CYCLES;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN ADC1_Init 2 */

  /* USER CODE END ADC1_Init 2 */

}

/**
  * @brief DAC Initialization Function
  * @param None
  * @retval None
  */
static void MX_DAC_Init(void)
{

  /* USER CODE BEGIN DAC_Init 0 */

  /* USER CODE END DAC_Init 0 */

  /* USER CODE BEGIN DAC_Init 1 */

  /* USER CODE END DAC_Init 1 */
  /* USER CODE BEGIN DAC_Init 2 */

  /* USER CODE END DAC_Init 2 */

}

/**
  * @brief I2C1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_I2C1_Init(void)
{

  /* USER CODE BEGIN I2C1_Init 0 */

  /* USER CODE END I2C1_Init 0 */

  /* USER CODE BEGIN I2C1_Init 1 */

  /* USER CODE END I2C1_Init 1 */
  hi2c1.Instance = I2C1;
  hi2c1.Init.ClockSpeed = 100000;
  hi2c1.Init.DutyCycle = I2C_DUTYCYCLE_2;
  hi2c1.Init.OwnAddress1 = 0;
  hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c1.Init.OwnAddress2 = 0;
  hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN I2C1_Init 2 */

  /* USER CODE END I2C1_Init 2 */

}

/**
  * @brief SPI1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_SPI1_Init(void)
{

  /* USER CODE BEGIN SPI1_Init 0 */

  /* USER CODE END SPI1_Init 0 */

  /* USER CODE BEGIN SPI1_Init 1 */

  /* USER CODE END SPI1_Init 1 */
  /* SPI1 parameter configuration*/
  hspi1.Instance = SPI1;
  hspi1.Init.Mode = SPI_MODE_MASTER;
  hspi1.Init.Direction = SPI_DIRECTION_2LINES;
  hspi1.Init.DataSize = SPI_DATASIZE_16BIT;
  hspi1.Init.CLKPolarity = SPI_POLARITY_LOW;
  hspi1.Init.CLKPhase = SPI_PHASE_1EDGE;
  hspi1.Init.NSS = SPI_NSS_SOFT;
  hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_32;
  hspi1.Init.FirstBit = SPI_FIRSTBIT_MSB;
  hspi1.Init.TIMode = SPI_TIMODE_DISABLE;
  hspi1.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
  hspi1.Init.CRCPolynomial = 10;
  if (HAL_SPI_Init(&hspi1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN SPI1_Init 2 */

  /* USER CODE END SPI1_Init 2 */

}

/**
  * @brief TIM3 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM3_Init(void)
{

  /* USER CODE BEGIN TIM3_Init 0 */

  /* USER CODE END TIM3_Init 0 */

  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};

  /* USER CODE BEGIN TIM3_Init 1 */

  /* USER CODE END TIM3_Init 1 */
  htim3.Instance = TIM3;
  htim3.Init.Prescaler = 0;
  htim3.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim3.Init.Period = 139;
  htim3.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim3.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
  if (HAL_TIM_Base_Init(&htim3) != HAL_OK)
  {
    Error_Handler();
  }
  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim3, &sClockSourceConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_UPDATE;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim3, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM3_Init 2 */

  /* USER CODE END TIM3_Init 2 */

}

/**
  * @brief TIM7 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM7_Init(void)
{

  /* USER CODE BEGIN TIM7_Init 0 */

  /* USER CODE END TIM7_Init 0 */

  TIM_MasterConfigTypeDef sMasterConfig = {0};

  /* USER CODE BEGIN TIM7_Init 1 */

  /* USER CODE END TIM7_Init 1 */
  htim7.Instance = TIM7;
  htim7.Init.Prescaler = 0;
  htim7.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim7.Init.Period = 139;
  htim7.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim7) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim7, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM7_Init 2 */

  /* USER CODE END TIM7_Init 2 */

}

/**
  * Enable DMA controller clock
  */
static void MX_DMA_Init(void)
{

  /* DMA controller clock enable */
  __HAL_RCC_DMA2_CLK_ENABLE();

  /* DMA interrupt init */
  /* DMA2_Stream0_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA2_Stream0_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(DMA2_Stream0_IRQn);

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  /* USER CODE BEGIN MX_GPIO_Init_1 */

  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOB, AD9833_CS1_Pin|AD9833_CS2_Pin, GPIO_PIN_SET);

  /*Configure GPIO pins : AD9833_CS1_Pin AD9833_CS2_Pin */
  GPIO_InitStruct.Pin = AD9833_CS1_Pin|AD9833_CS2_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */
/*
 * Goertzel 算法 —— 计算信号在指定频率处的 DFT 幅度
 *
 * 原理简介：
 *   标准 DFT 需要逐点乘 cos(ωn) 和 sin(ωn)，每个频率要算 2N 次三角函数，太慢。
 *   Goertzel 用一个二阶 IIR 谐振器替代：
 *       s[n] = x[n] + 2·cos(ω)·s[n-1] - s[n-2]
 *   迭代 N 次后，从两个状态变量 s1、s2 直接得到幅度平方：
 *       |X(ω)|² = s1² + s2² − 2·cos(ω)·s1·s2
 *   整个算法只需要 1 次 cosf() 调用，其余全是乘加运算。
 *
 * 参数：
 *   x    - 输入信号数组（已去直流，以 0 为中心）
 *   N    - 采样点数（1024）
 *   freq - 目标频率（Hz），如 20000.0f
 *   fs   - 采样率（Hz），600000.0f
 * 返回：
 *   该频率处的 DFT 幅度（非归一化），用于幅度比较
 */
static float goertzel(float *x, int N, float freq, float fs)
{
    /* 计算角频率 ω = 2π·f/fs，以及 IIR 系数 coeff = 2·cos(ω) */
    float omega = 2.0f * M_PI * freq / fs;
    float coeff = 2.0f * cosf(omega);

    /* s0=当前值 s[n], s1=s[n-1], s2=s[n-2] */
    float s0 = 0.0f, s1 = 0.0f, s2 = 0.0f;

    /* IIR 谐振器迭代：s[n] = x[n] + coeff·s[n-1] - s[n-2] */
    for (int i = 0; i < N; i++) {
        s0 = x[i] + coeff * s1 - s2;
        s2 = s1;
        s1 = s0;
    }

    /* 迭代结束后：s1 = s[N-1], s2 = s[N-2]
     * 幅度平方 = s1² + s2² - coeff·s1·s2
     * （此公式由 |X|² = Re² + Im² 推导化简而来，省去了显式计算实部虚部） */
    float mag_sq = s1 * s1 + s2 * s2 - coeff * s1 * s2;
    if (mag_sq < 0.0f) mag_sq = 0.0f;   /* 浮点舍入可能导致微小负数，钳位到 0 */
    return sqrtf(mag_sq);
}

/*
 * 冒泡排序找出 DFT 幅度最大的两个频率
 *
 * 采用索引排序（不改变原始数据顺序）：
 *   1. 建立索引数组 indices = {0, 1, ..., 8}
 *   2. 按 mags[indices[j]] 从大到小冒泡排序索引数组
 *   3. 取前两个索引对应的频率，确保 fa < fb
 *
 * 参数：
 *   mags  - 9 个频率的 DFT 幅度数组
 *   freqs - 对应的频率值数组（Hz）
 *   N     - 数组长度（9）
 *   fa    - 输出：频率较低的那个（Hz）
 *   fb    - 输出：频率较高的那个（Hz）
 */
static void find_top_two(float *mags, const uint32_t *freqs, int N,
                         uint32_t *fa, uint32_t *fb)
{
    /* 初始化索引数组 */
    int indices[NUM_FREQS];
    for (int i = 0; i < N; i++) indices[i] = i;

    /* 冒泡排序：按幅度从大到小排列索引 */
    for (int i = 0; i < N - 1; i++) {
        for (int j = 0; j < N - 1 - i; j++) {
            if (mags[indices[j]] < mags[indices[j + 1]]) {
                int tmp = indices[j];
                indices[j] = indices[j + 1];
                indices[j + 1] = tmp;
            }
        }
    }

    /* 取排序后前两个索引对应的频率 */
    uint32_t f1 = freqs[indices[0]];   /* 幅度最大 */
    uint32_t f2 = freqs[indices[1]];   /* 幅度第二大 */
    *fa = (f1 < f2) ? f1 : f2;         /* 较小者赋给 fa */
    *fb = (f1 < f2) ? f2 : f1;         /* 较大者赋给 fb */
}
/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */

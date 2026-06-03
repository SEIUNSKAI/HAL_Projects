/*
 * ad9833.c —— 双路 AD9833 DDS 模块驱动（硬件 SPI）
 *
 * 接线：
 *   PB0 → CS1 (模块1, 输出 fa)    PB1 → CS2 (模块2, 输出 fb)
 *   SPI1: PA5=SCLK, PA7=MOSI     (PA6=MISO 未使用，AD9833 只收不发)
 *
 * AD9833 SPI 协议：
 *   每次传 16bit，MSB 先发。D15-D14 选寄存器：
 *     00=控制, 01=FREQ0, 10=FREQ1, 11=PHASE
 *   28bit 频率字分两次写（先低14位, 后高14位），AD9833 自动拼接。
 *
 * 参考：康威电子 AD9833 模块标准库例程（F1/软件SPI）
 *       改编为 F407 HAL 硬件 SPI 版本
 */

#include "ad9833.h"

/* ---- 片选宏 ---- */
#define CS1_LOW()   HAL_GPIO_WritePin(AD9833_CS1_GPIO_Port, AD9833_CS1_Pin, GPIO_PIN_RESET)
#define CS1_HIGH()  HAL_GPIO_WritePin(AD9833_CS1_GPIO_Port, AD9833_CS1_Pin, GPIO_PIN_SET)
#define CS2_LOW()   HAL_GPIO_WritePin(AD9833_CS2_GPIO_Port, AD9833_CS2_Pin, GPIO_PIN_RESET)
#define CS2_HIGH()  HAL_GPIO_WritePin(AD9833_CS2_GPIO_Port, AD9833_CS2_Pin, GPIO_PIN_SET)

/* ---- 外部 HAL 句柄 ---- */
extern SPI_HandleTypeDef hspi1;

/* ---- 寄存器地址 ---- */
#define REG_CMD    0x0000   /* D15-D14 = 00 */
#define REG_FREQ0  0x4000   /* D15-D14 = 01 */
#define REG_FREQ1  0x8000   /* D15-D14 = 10 */

/* ---- 控制寄存器位 ---- */
#define CMD_B28     (1 << 13)   /* 自动频率写入模式 */
#define CMD_RESET   (1 << 8)    /* 复位 */

/* ---- 波形类型（控制寄存器低位组合） ---- */
#define WAVE_SINE   ((0 << 5) | (0 << 1) | (0 << 3))

/* ---- 内部函数：向指定模块发一个 16bit SPI 字 ---- */
static void AD9833_WriteReg(uint8_t module, uint16_t data)
{
    if (module == 1) {
        CS1_LOW();
        HAL_SPI_Transmit(&hspi1, (uint8_t *)&data, 1, 10);
        CS1_HIGH();
    } else {
        CS2_LOW();
        HAL_SPI_Transmit(&hspi1, (uint8_t *)&data, 1, 10);
        CS2_HIGH();
    }
}

/* ---- 公开 API ---- */

/*
 * 初始化两路 AD9833 模块：
 *   复位 → 写频率0 → 解除复位 → 开始输出正弦波
 */
void AD9833_Init(void)
{
    /* 确保 SPI 为 16bit 模式（AD9833 要求每次传 16bit） */
    hspi1.Init.DataSize = SPI_DATASIZE_16BIT;
    HAL_SPI_Init(&hspi1);

    /* 初始状态：CS 高（不选中） */
    CS1_HIGH();
    CS2_HIGH();

    /* 两片模块统一初始化：复位 + 写 0 频率 + 启动 */
    for (uint8_t m = 1; m <= 2; m++) {
        /* ① 复位芯片 */
        AD9833_WriteReg(m, REG_CMD | CMD_RESET);

        /* ② 写入频率 0（28bit = 0） */
        AD9833_WriteReg(m, REG_CMD | CMD_B28 | WAVE_SINE);   /* 控制字：B28=1, 正弦 */
        AD9833_WriteReg(m, REG_FREQ0 | 0);                    /* FREQ0 低14位 = 0 */
        AD9833_WriteReg(m, REG_FREQ0 | 0);                    /* FREQ0 高14位 = 0 */

        /* ③ 解除复位，开始输出（FSEL=0 即使用 FREQ0） */
        AD9833_WriteReg(m, REG_CMD | WAVE_SINE);
    }
}

/*
 * 设置指定模块的输出频率
 *   module  : 1 或 2（对应 PB0/PB1 片选）
 *   freq_hz : 目标频率（Hz），有效范围约 0 ~ 12.5MHz
 *
 * 流程：复位 → 写 B28 控制字 → 写 FREQ0 低14位 → 写 FREQ0 高14位 → 启动
 */
void AD9833_SetFreq(uint8_t module, uint32_t freq_hz)
{
    if (module < 1 || module > 2) return;

    /* 频率字 = f_out × 2^28 ÷ 25MHz */
    uint32_t fw = (uint32_t)((uint64_t)freq_hz * (1ULL << 28) / AD9833_MCLK);

    /* ① 复位（保证频率写入时不被触发干扰） */
    AD9833_WriteReg(module, REG_CMD | CMD_RESET);

    /* ② B28=1：启用自动频率写入（两次写 FREQ0 自动拼接 28bit） */
    AD9833_WriteReg(module, REG_CMD | CMD_B28 | WAVE_SINE);

    /* ③ 写 28bit 频率字：先低 14 位，后高 14 位 */
    AD9833_WriteReg(module, REG_FREQ0 | (fw & 0x3FFF));           /* D13-D0 = 低14位 */
    AD9833_WriteReg(module, REG_FREQ0 | ((fw >> 14) & 0x3FFF));   /* D13-D0 = 高14位 */

    /* ④ 解除复位 → 正弦波立即从 VOUT 引脚输出 */
    AD9833_WriteReg(module, REG_CMD | WAVE_SINE);
}

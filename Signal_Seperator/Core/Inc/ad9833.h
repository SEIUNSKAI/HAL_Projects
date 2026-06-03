#ifndef __AD9833_H
#define __AD9833_H

#include "main.h"

#define AD9833_MCLK  25000000U

void AD9833_Init(void);
void AD9833_SetFreq(uint8_t module, uint32_t freq_hz);

#endif

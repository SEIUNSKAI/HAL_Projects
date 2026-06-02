#ifndef __OSCILLOSCOPE_H
#define __OSCILLOSCOPE_H

#include <stdint.h>
#include "stm32f1xx_hal.h"

#define SAMPLE_POINTS 128

void Oscilloscope_Init(void);
void Oscilloscope_ADC_Collect(void);
void Oscilloscope_Draw(void);

#endif
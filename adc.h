/*
 * adc.h - adc setup
 */

#ifndef __adc__
#define __adc__

#include "stm32f4xx.h"
#include "stm32f4xx_adc.h"
#include "stm32f4xx_dma.h"
#include "stm32f4xx_gpio.h"
#include "stm32f4xx_rcc.h"
#include "misc.h"

#define MIN_ADC_CHANGE 20
#define MIN_SCALE_ADC_CHANGE 100

void ADC1_Init(uint16_t *ADC_Buffer);
void ADC3_Init(uint16_t *ADC_Buffer);
//uint32_t check_ADC3(void);

#endif

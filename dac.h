/*
 * dac.h
 *
 *  Created on: Jul 28, 2014
 *      Author: design
 */

#ifndef DAC_H_
#define DAC_H_

void DAC_Ch1_NoiseConfig(void);
void TIM6_Config(void);
void update_TIM6(uint32_t period);

#endif /* DAC_H_ */

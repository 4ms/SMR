/*
 * limiter.h
 *
 *  Created on: Aug 3, 2016
 *      Author: design
 */

#ifndef LIMITER_H_
#define LIMITER_H_
#include <stm32f4xx.h>

#define C_THRESHOLD_75_16BIT 201326592 /*  ((1<<15)*(1<<15)*0.75*0.75 - 0.75*(1<<15)*(1<<15))  */
#define C_THRESHOLD_75_32BIT 864691128455135232 /*  ((1<<31)*(1<<31)*0.75*0.75 - 0.75*(1<<31)*(1<<31))
(1<<58 * 9) - (1<<60 * 3)
2594073385365405696 - 3458764513820540928
=-864691128455135232
*/

int32_t limiter(int32_t val);

void init_limiter(uint32_t max_sample_val, float threshold_percent);

#endif /* LIMITER_H_ */
/*
 * rotary.h
 *
 * Copyright 2011 Ben Buxton. Licenced under the GNU GPL Version 3.
 * Contact: bb@cactii.net
 *
 * Conversion for use with STM32, in 2015 by: Dan Green (danngreen1@gmail.com)
 *
 */

#ifndef ROTARY_H_
#define ROTARY_H_

#include <stm32f4xx.h>


// Enable this to emit codes twice per step.
#define HALF_STEP

// Values returned by 'process'
// No complete step yet.
#define DIR_NONE 0x0
// Clockwise step.
#define DIR_CW 0x10
// Anti-clockwise step.
#define DIR_CCW 0x20


#define ROTARY_GPIO GPIOD
#define ROTARY_RCC RCC_AHB1Periph_GPIOD

#define ROTARY_A_pin GPIO_Pin_11
#define ROTARY_B_pin GPIO_Pin_10
#define ROTARY_A (ROTARY_GPIO->IDR & ROTARY_A_pin)
#define ROTARY_B (ROTARY_GPIO->IDR & ROTARY_B_pin)

#define ROTARY_SW_pin GPIO_Pin_9
#define ROTARY_SW (!(ROTARY_GPIO->IDR & ROTARY_SW_pin))

void init_rotary(void);
uint8_t read_rotary(void);

#endif /* ROTARY_H_ */

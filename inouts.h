/*
 * inouts.h
 */

#ifndef INOUTS_H_
#define INOUTS_H_

#include "stm32f4xx.h"

#define BUTTON_RCC RCC_AHB1Periph_GPIOD

#define READ_BUTTON(x,y) (!(x->IDR & y))

#define ENV_MODE_pin GPIO_Pin_8
#define ENV_MODE_GPIO GPIOD
#define ENV_MODE (!(ENV_MODE_GPIO->IDR & ENV_MODE_pin))

#define LOCK0_pin GPIO_Pin_12
#define LOCK0_GPIO GPIOD
#define LOCK0 (!(GPIO_ReadInputDataBit(LOCK0_GPIO, LOCK0_pin)))

#define LOCK5_pin GPIO_Pin_13
#define LOCK5_GPIO GPIOD
#define LOCK5 (!(GPIO_ReadInputDataBit(LOCK5_GPIO, LOCK5_pin)))

#define ROTUP_pin GPIO_Pin_15
#define ROTUP_GPIO GPIOC
#define ROTUP (!(GPIO_ReadInputDataBit(ROTUP_GPIO, ROTUP_pin)))

#define ROTDOWN_pin GPIO_Pin_14
#define ROTDOWN_GPIO GPIOC
#define ROTDOWN (!(GPIO_ReadInputDataBit(ROTDOWN_GPIO, ROTDOWN_pin)))

#define CVLAG_pin GPIO_Pin_8
#define CVLAG_GPIO GPIOE
#define CVLAG (GPIO_ReadInputDataBit(CVLAG_GPIO, CVLAG_pin))

#define ENVSPEED_pin GPIO_Pin_9
#define ENVSPEED_GPIO GPIOE
#define ENVSPEED (!(GPIO_ReadInputDataBit(ENVSPEED_GPIO, ENVSPEED_pin)))


#define RANGE_pin GPIO_Pin_14
#define RANGE_GPIO GPIOD
#define RANGE_MODE (!(GPIO_ReadInputDataBit(RANGE_GPIO, RANGE_pin)))


#define LED_RCC RCC_AHB1Periph_GPIOE

#define LED_CLIP1 GPIO_Pin_10
#define LED_CLIP2 GPIO_Pin_11
#define LED_CLIP3 GPIO_Pin_12
#define LED_CLIP4 GPIO_Pin_13
#define LED_CLIP5 GPIO_Pin_14
#define LED_CLIP6 GPIO_Pin_15


#define LED_INCLIP GPIO_Pin_7

#define LED_LOCK0 GPIO_Pin_5
#define LED_LOCK5 GPIO_Pin_6

#define LED_RING_OE GPIO_Pin_4

#define LED_GPIO GPIOE

#define LED_OFF(x) LED_GPIO->BSRRH = x
#define LED_ON(x) LED_GPIO->BSRRL = x

#define DEBUG0 GPIO_Pin_0
#define DEBUG1 GPIO_Pin_1
#define DEBUG2 GPIO_Pin_2
#define DEBUG3 GPIO_Pin_3
#define DEBUG_GPIO GPIOE
#define DEBUG_RCC RCC_AHB1Periph_GPIOE
#define DEBUG_OFF(x) DEBUG_GPIO->BSRRH = x
#define DEBUG_ON(x) DEBUG_GPIO->BSRRL = x


#define ENVOUT_PWM_pins_1256 (GPIO_Pin_0 | GPIO_Pin_1 | GPIO_Pin_4 | GPIO_Pin_5)
#define ENVOUT_PWM_GPIO_1256 GPIOB
#define ENVOUT_PWM_RCC_1256 RCC_AHB1Periph_GPIOB
#define ENVOUT_PWM_pinsource_1 GPIO_PinSource0
#define ENVOUT_PWM_pinsource_2 GPIO_PinSource1
#define ENVOUT_PWM_pinsource_5 GPIO_PinSource4
#define ENVOUT_PWM_pinsource_6 GPIO_PinSource5

#define ENVOUT_PWM_pins_34 (GPIO_Pin_8 | GPIO_Pin_10)
#define ENVOUT_PWM_GPIO_34 GPIOA
#define ENVOUT_PWM_RCC_34 RCC_AHB1Periph_GPIOA
#define ENVOUT_PWM_pinsource_3 GPIO_PinSource8
#define ENVOUT_PWM_pinsource_4 GPIO_PinSource10


#define NUM_ADCS 12
#define MORPH_ADC 0
#define QVAL_ADC 1
//2,3,4,5,6,7 are sliders
#define FREQNUDGE1_ADC 8
#define FREQNUDGE6_ADC 9
#define SCALE_ADC 10
#define SPREAD_ADC 11

/* ADC:
  	GPIOA: GPIO_Pin_7 | GPIO_Pin_6 | GPIO_Pin_3 | GPIO_Pin_2 | GPIO_Pin_1 | GPIO_Pin_0;

	GPIOC: GPIO_Pin_3 | GPIO_Pin_2 | GPIO_Pin_1 | GPIO_Pin_0;

 */
void init_inouts(void);

#endif /* INOUTS_H_ */

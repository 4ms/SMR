/*
 * inouts.h
 */

#ifndef INOUTS_H_
#define INOUTS_H_

#include "stm32f4xx.h"

#define BUTTON_RCC RCC_AHB1Periph_GPIOD
#define BUTTON2_RCC RCC_AHB1Periph_GPIOB

#define ENV_MODE_pin GPIO_Pin_8
#define ENV_MODE_GPIO GPIOD
#define ENV_MODE (!(ENV_MODE_GPIO->IDR & ENV_MODE_pin))

#define LOCK1_pin GPIO_Pin_12
#define LOCK2_pin GPIO_Pin_0
#define LOCK3_pin GPIO_Pin_1
#define LOCK4_pin GPIO_Pin_2
#define LOCK5_pin GPIO_Pin_3
#define LOCK6_pin GPIO_Pin_13
#define LOCKBUT_GPIO GPIOD

#define LOCK1 (!(LOCKBUT_GPIO->IDR & LOCK0_pin))
#define LOCK2 (!(LOCKBUT_GPIO->IDR & LOCK0_pin))
#define LOCK3 (!(LOCKBUT_GPIO->IDR & LOCK0_pin))
#define LOCK4 (!(LOCKBUT_GPIO->IDR & LOCK0_pin))
#define LOCK5 (!(LOCKBUT_GPIO->IDR & LOCK0_pin))
#define LOCK6 (!(LOCKBUT_GPIO->IDR & LOCK0_pin))

#define LOCK135_pin GPIO_Pin_6
#define LOCK135_GPIO GPIOB
#define LOCK135 (LOCK135_GPIO->IDR & LOCK135_pin)

#define LOCK246_pin GPIO_Pin_7
#define LOCK246_GPIO GPIOB
#define LOCK246 (LOCK246_GPIO->IDR & LOCK246_pin)

#define MOD135_pin GPIO_Pin_2
#define MOD135_GPIO GPIOB
#define MOD135 (!(MOD135_GPIO->IDR & MOD135_pin))

#define MOD246_pin GPIO_Pin_15
#define MOD246_GPIO GPIOD
#define MOD246 (!(MOD246_GPIO->IDR & MOD246_pin))

#define ROT_RCC RCC_AHB1Periph_GPIOC

#define ROTUP_pin GPIO_Pin_15
#define ROTUP_GPIO GPIOC
#define ROTUP (ROTUP_GPIO->IDR & ROTUP_pin)

#define ROTDOWN_pin GPIO_Pin_14
#define ROTDOWN_GPIO GPIOC
#define ROTDOWN (ROTDOWN_GPIO->IDR & ROTDOWN_pin)

#define CVLAG_pin GPIO_Pin_8
#define CVLAG_GPIO GPIOE
#define CVLAG ((CVLAG_GPIO->IDR & CVLAG_pin))

#define ENVSPEEDSLOW_pin GPIO_Pin_9
#define ENVSPEEDSLOW_GPIO GPIOE
#define ENVSPEEDSLOW (!(ENVSPEEDSLOW_GPIO->IDR & ENVSPEEDSLOW_pin))

#define ENVSPEEDFAST_pin GPIO_Pin_3
#define ENVSPEEDFAST_GPIO GPIOB
#define ENVSPEEDFAST (!(ENVSPEEDFAST_GPIO->IDR & ENVSPEEDFAST_pin))


#define RANGE_pin GPIO_Pin_14
#define RANGE_GPIO GPIOD
#define RANGE_MODE ((RANGE_GPIO->IDR & RANGE_pin))


#define LED_RCC RCC_AHB1Periph_GPIOE
#define LED_GPIO GPIOE
#define LED_OFF(x) LED_GPIO->BSRRH = x
#define LED_ON(x) LED_GPIO->BSRRL = x

#define LED_CLIP1 GPIO_Pin_15
#define LED_CLIP2 GPIO_Pin_14
#define LED_CLIP3 GPIO_Pin_13
#define LED_CLIP4 GPIO_Pin_12
#define LED_CLIP5 GPIO_Pin_11
#define LED_CLIP6 GPIO_Pin_10

#define LED_INCLIPL GPIO_Pin_7

#define LED_LOCK1 GPIO_Pin_5
#define LED_LOCK2 GPIO_Pin_0
#define LED_LOCK3 GPIO_Pin_1
#define LED_LOCK4 GPIO_Pin_2
#define LED_LOCK5 GPIO_Pin_3
#define LED_LOCK6 GPIO_Pin_6

#define LED_RING_OE GPIO_Pin_4

#define LED_INCLIPR GPIO_Pin_7
#define LED_INCLIPR_GPIO GPIOC

#define LED_CLIPL_ON LED_GPIO->BSRRL = LED_INCLIPL
#define LED_CLIPL_OFF LED_GPIO->BSRRH = LED_INCLIPL

#define LED_CLIPR_RCC RCC_AHB1Periph_GPIOC

#define LED_CLIPR_ON LED_INCLIPR_GPIO->BSRRL = LED_INCLIPR
#define LED_CLIPR_OFF LED_INCLIPR_GPIO->BSRRH = LED_INCLIPR


#define DEBUG0 GPIO_Pin_5
#define DEBUG1 GPIO_Pin_6
#define DEBUG2 GPIO_Pin_7

#define DEBUGA_GPIO GPIOD
#define DEBUGA_RCC RCC_AHB1Periph_GPIOD
#define DEBUGA_OFF(x) DEBUGA_GPIO->BSRRH = x
#define DEBUGA_ON(x) DEBUGA_GPIO->BSRRL = x

#define DEBUG3 GPIO_Pin_12
#define DEBUGB_GPIO GPIOC
#define DEBUGB_RCC RCC_AHB1Periph_GPIOC
#define DEBUG3_OFF DEBUGB_GPIO->BSRRH = DEBUG3
#define DEBUG3_ON DEBUGB_GPIO->BSRRL = DEBUG3

#define DEBUG4 GPIO_Pin_6
#define DEBUGC_GPIO GPIOF
#define DEBUGC_RCC RCC_AHB1Periph_GPIOF
#define DEBUG4_OFF DEBUGC_GPIO->BSRRH = DEBUG4
#define DEBUG4_ON DEBUGC_GPIO->BSRRL = DEBUG4

#define ENVOUT_PWM_pins_12 (GPIO_Pin_8 | GPIO_Pin_9)
#define ENVOUT_PWM_GPIO_12 GPIOC
#define ENVOUT_PWM_RCC_12 RCC_AHB1Periph_GPIOC
#define ENVOUT_PWM_pinsource_1 GPIO_PinSource8
#define ENVOUT_PWM_pinsource_2 GPIO_PinSource9
#define ENVOUT_PWM_CC_1 CCR3
#define ENVOUT_PWM_CC_2 CCR4

#define ENVOUT_PWM_pins_34 (GPIO_Pin_8 | GPIO_Pin_10)
#define ENVOUT_PWM_GPIO_34 GPIOA
#define ENVOUT_PWM_RCC_34 RCC_AHB1Periph_GPIOA
#define ENVOUT_PWM_pinsource_3 GPIO_PinSource8
#define ENVOUT_PWM_pinsource_4 GPIO_PinSource10
#define ENVOUT_PWM_CC_3 CCR1
#define ENVOUT_PWM_CC_4 CCR3

#define ENVOUT_PWM_pins_56 (GPIO_Pin_4 | GPIO_Pin_5)
#define ENVOUT_PWM_GPIO_56 GPIOB
#define ENVOUT_PWM_RCC_56 RCC_AHB1Periph_GPIOB
#define ENVOUT_PWM_pinsource_5 GPIO_PinSource4
#define ENVOUT_PWM_pinsource_6 GPIO_PinSource5
#define ENVOUT_PWM_CC_5 CCR1
#define ENVOUT_PWM_CC_6 CCR2


/*
 ADC:
  	GPIOA: GPIO_Pin_6 | GPIO_Pin_7
	GPIOB: GPIO_Pin_0 | GPIO_Pin_1
	GPIOC: GPIO_Pin_1 | GPIO_Pin_2 | GPIO_Pin_3 | GPIO_Pin_4
 */
#define NUM_ADCS 15
#define MORPH_ADC 0
#define QVAL_ADC 1
#define LEVEL_ADC_BASE 2
//2,3,4,5,6,7 are sliders
#define FREQNUDGE1_ADC 8
#define FREQNUDGE6_ADC 9
#define SCALE_ADC 10
#define SPREAD_ADC 11
#define ROTCV_ADC 12
#define FM_135_ADC 13
#define FM_246_ADC 14
#define QPOT_ADC 15

void init_inouts(void);
void init_inputread_timer(void);
inline void LOCKLED_ON(int led);
inline void LOCKLED_OFF(int led);
inline uint8_t LOCKBUTTON(uint8_t x);
void poll_switches(void);

#endif /* INOUTS_H_ */

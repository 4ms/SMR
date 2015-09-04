/*
 * dig_inouts.h - setup digital input and output pins
 *
 * Author: Dan Green (danngreen1@gmail.com)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 * See http://creativecommons.org/licenses/MIT/ for more information.
 *
 * -----------------------------------------------------------------------------
 */

#ifndef INOUTS_H_
#define INOUTS_H_

#include <stm32f4xx.h>

#define BUTTON_RCC (RCC_AHB1Periph_GPIOD | RCC_AHB1Periph_GPIOB | RCC_AHB1Periph_GPIOG)

#define ENV_MODE_pin GPIO_Pin_8
#define ENV_MODE_GPIO GPIOD
#define ENV_MODE (!(ENV_MODE_GPIO->IDR & ENV_MODE_pin))

#define LOCK1_pin GPIO_Pin_12
#define LOCK2_pin GPIO_Pin_0
#define LOCK3_pin GPIO_Pin_1
#define LOCK4_pin GPIO_Pin_2
#define LOCK5_pin GPIO_Pin_3
#define LOCKBUT_GPIO GPIOD

#define LOCKBUT6_GPIO GPIOG
#define LOCK6_pin GPIO_Pin_15


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

#define ENVSPEEDSLOW_pin GPIO_Pin_3
#define ENVSPEEDSLOW_GPIO GPIOB
#define ENVSPEEDSLOW (!(ENVSPEEDSLOW_GPIO->IDR & ENVSPEEDSLOW_pin))

#define ENVSPEEDFAST_pin GPIO_Pin_9
#define ENVSPEEDFAST_GPIO GPIOE
#define ENVSPEEDFAST (!(ENVSPEEDFAST_GPIO->IDR & ENVSPEEDFAST_pin))


#define RANGE_pin GPIO_Pin_14
#define RANGE_GPIO GPIOD
#define RANGE_MODE ((RANGE_GPIO->IDR & RANGE_pin))


#define LED_RCC RCC_AHB1Periph_GPIOE
#define LED_GPIO GPIOE
#define LED_OFF(x) LED_GPIO->BSRRH = x
#define LED_ON(x) LED_GPIO->BSRRL = x

#define LED_SLIDER_RCC RCC_AHB1Periph_GPIOG
#define LED_SLIDER_GPIO GPIOG
#define LED_SLIDER_OFF(x) LED_SLIDER_GPIO->BSRRH = x
#define LED_SLIDER_ON(x) LED_SLIDER_GPIO->BSRRL = x

#define LED_SLIDER1 GPIO_Pin_9
#define LED_SLIDER2 GPIO_Pin_10
#define LED_SLIDER3 GPIO_Pin_11
#define LED_SLIDER4 GPIO_Pin_12
#define LED_SLIDER5 GPIO_Pin_13
#define LED_SLIDER6 GPIO_Pin_14

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


#define DEBUG0 GPIO_Pin_2
#define DEBUG1 GPIO_Pin_3
#define DEBUG2 GPIO_Pin_5
#define DEBUG3 GPIO_Pin_6

#define DEBUGA_GPIO GPIOG
#define DEBUGA_RCC RCC_AHB1Periph_GPIOG
#define DEBUGA_OFF(x) DEBUGA_GPIO->BSRRH = x
#define DEBUGA_ON(x) DEBUGA_GPIO->BSRRL = x

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
	GPIOF: GPIO_Pin_3 | GPIO_Pin_4 | GPIO_Pin_5 | GPIO_Pin_6 | GPIO_Pin_7 | GPIO_Pin_8 | GPIO_Pin_9
 */
#define NUM_ADCS 15

#define MORPH_ADC 0
#define QVAL_ADC 1
#define LEVEL_ADC_BASE 2
//2,3,4,5,6,7 are Level CV jacks
#define FREQNUDGE1_ADC 8
#define FREQNUDGE6_ADC 9
#define SCALE_ADC 10
#define SPREAD_ADC 11
#define ROTCV_ADC 12
#define FREQCV1_ADC 13
#define FREQCV6_ADC 14

#define NUM_ADC3S 7

#define QPOT_ADC 0
#define SLIDER_ADC_BASE 1
//1-6 are sliders

void init_inouts(void);
void init_inputread_timer(void);
inline void LOCKLED_ON(int led);
inline void LOCKLED_OFF(int led);
inline void LOCKLED_ALLON(void);
inline void LOCKLED_ALLOFF(void);


inline uint8_t LOCKBUTTON(uint8_t x);

#endif /* INOUTS_H_ */

/*
 * dig_inouts.c - setup digital input and output pins
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

#include "dig_inouts.h"
#include "rotary.h"
#include "globals.h"

uint8_t do_ROTDOWN;
uint8_t do_ROTUP;
uint8_t do_LOCK135;
uint8_t do_LOCK246;
uint8_t do_ROTA;
uint8_t do_ROTB;

const int LED_LOCK[6]={LED_LOCK1, LED_LOCK2, LED_LOCK3, LED_LOCK4, LED_LOCK5, LED_LOCK6};
inline void LOCKLED_ON(int led){
	LED_GPIO->BSRRL = LED_LOCK[led];
}
inline void LOCKLED_OFF(int led){
	LED_GPIO->BSRRH = LED_LOCK[led];
}
inline void LOCKLED_ALLON(void){
	LED_GPIO->BSRRL = LED_LOCK1 | LED_LOCK2 | LED_LOCK3 | LED_LOCK4 | LED_LOCK5 | LED_LOCK6;
}
inline void LOCKLED_ALLOFF(void){
	LED_GPIO->BSRRH = LED_LOCK1 | LED_LOCK2 | LED_LOCK3 | LED_LOCK4 | LED_LOCK5 | LED_LOCK6;
}

const int _lockbutton[6]={LOCK1_pin, LOCK2_pin, LOCK3_pin, LOCK4_pin, LOCK5_pin, LOCK6_pin};
inline uint8_t LOCKBUTTON(uint8_t x){
	if (x!=5) return (!(LOCKBUT_GPIO->IDR & _lockbutton[x]));
	else return (!(LOCKBUT6_GPIO->IDR & _lockbutton[5]));
}

void init_inouts(void){
	GPIO_InitTypeDef gpio;
	int i;


	// Set up Outputs:

	RCC_AHB1PeriphClockCmd(LED_RCC, ENABLE);
	RCC_AHB1PeriphClockCmd(LED_CLIPR_RCC, ENABLE);

	GPIO_StructInit(&gpio);
	gpio.GPIO_Pin = LED_LOCK1 | LED_LOCK6 | LED_RING_OE;
	gpio.GPIO_Mode = GPIO_Mode_OUT;
	gpio.GPIO_Speed = GPIO_Speed_2MHz;
	gpio.GPIO_OType = GPIO_OType_PP;
	gpio.GPIO_PuPd = GPIO_PuPd_NOPULL;
	GPIO_Init(LED_GPIO, &gpio);

	gpio.GPIO_Pin = LED_LOCK1 | LED_LOCK2 | LED_LOCK3 | LED_LOCK4 | LED_LOCK5 | LED_LOCK6;
	GPIO_Init(LED_GPIO, &gpio);

	gpio.GPIO_Pin = LED_INCLIPL;
	GPIO_Init(LED_GPIO, &gpio);

	gpio.GPIO_Pin = LED_INCLIPR;
	GPIO_Init(LED_INCLIPR_GPIO, &gpio);

	RCC_AHB1PeriphClockCmd(LED_SLIDER_RCC, ENABLE);
	gpio.GPIO_Pin = LED_SLIDER1 | LED_SLIDER2 | LED_SLIDER3 | LED_SLIDER4  | LED_SLIDER5 | LED_SLIDER6 ;
	GPIO_Init(LED_SLIDER_GPIO, &gpio);


	RCC_AHB1PeriphClockCmd(DEBUGA_RCC, ENABLE);
	gpio.GPIO_Pin = DEBUG0 | DEBUG1 | DEBUG2 | DEBUG3;
	gpio.GPIO_Mode = GPIO_Mode_OUT;
	gpio.GPIO_Speed = GPIO_Speed_2MHz;
	gpio.GPIO_OType = GPIO_OType_PP;
	gpio.GPIO_PuPd = GPIO_PuPd_NOPULL;
	GPIO_Init(DEBUGA_GPIO, &gpio);

	//Set up Inputs:

	RCC_AHB1PeriphClockCmd(BUTTON_RCC, ENABLE);

	RCC_AHB1PeriphClockCmd(ROT_RCC, ENABLE);


	gpio.GPIO_Mode = GPIO_Mode_IN;
	gpio.GPIO_Speed = GPIO_Speed_2MHz;
	gpio.GPIO_PuPd = GPIO_PuPd_UP;

	gpio.GPIO_Pin = ENV_MODE_pin;
	GPIO_Init(ENV_MODE_GPIO, &gpio);

	gpio.GPIO_Pin = ENVSPEEDSLOW_pin;
	GPIO_Init(ENVSPEEDSLOW_GPIO, &gpio);

	gpio.GPIO_Pin = ENVSPEEDFAST_pin;
	GPIO_Init(ENVSPEEDFAST_GPIO, &gpio);

	gpio.GPIO_Pin = LOCK1_pin | LOCK2_pin | LOCK3_pin | LOCK4_pin | LOCK5_pin;
	GPIO_Init(LOCKBUT_GPIO, &gpio);

	gpio.GPIO_Pin = LOCK6_pin;
	GPIO_Init(LOCKBUT6_GPIO, &gpio);

	gpio.GPIO_Pin = RANGE_pin;
	GPIO_Init(RANGE_GPIO, &gpio);

	gpio.GPIO_Pin = CVLAG_pin;
	GPIO_Init(CVLAG_GPIO, &gpio);

	gpio.GPIO_Pin = MOD135_pin;
	GPIO_Init(MOD135_GPIO, &gpio);

	gpio.GPIO_Pin = MOD246_pin;
	GPIO_Init(MOD246_GPIO, &gpio);

	gpio.GPIO_PuPd = GPIO_PuPd_DOWN;

	gpio.GPIO_Pin = ROTUP_pin;
	GPIO_Init(ROTUP_GPIO, &gpio);

	gpio.GPIO_Pin = ROTDOWN_pin;
	GPIO_Init(ROTDOWN_GPIO, &gpio);

	gpio.GPIO_Pin = LOCK135_pin;
	GPIO_Init(LOCK135_GPIO, &gpio);

	gpio.GPIO_Pin = LOCK246_pin;
	GPIO_Init(LOCK246_GPIO, &gpio);

	init_inputread_timer();
}

void init_inputread_timer(void){
	TIM_TimeBaseInitTypeDef  tim;

	uint16_t PrescalerValue = 0;

	NVIC_InitTypeDef nvic;

	RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM4, ENABLE);

	nvic.NVIC_IRQChannel = TIM4_IRQn;
	nvic.NVIC_IRQChannelPreemptionPriority = 3;
	nvic.NVIC_IRQChannelSubPriority = 2;
	nvic.NVIC_IRQChannelCmd = ENABLE;
	NVIC_Init(&nvic);

	TIM_TimeBaseStructInit(&tim);
	//tim.TIM_Period = 16000; // 16000 is 5.5kHz or every 0.17778ms
	tim.TIM_Period = 32000;
	tim.TIM_Prescaler = 0;
	tim.TIM_ClockDivision = 0;
	tim.TIM_CounterMode = TIM_CounterMode_Up;

	TIM_TimeBaseInit(TIM4, &tim);

	TIM_ITConfig(TIM4, TIM_IT_Update, ENABLE);

	TIM_Cmd(TIM4, ENABLE);
}

uint16_t State[6] = {0,0,0,0,0,0}; // Current debounce status

#define debounce_trigger_inputs TIM4_IRQHandler

void debounce_trigger_inputs(void)
{
	uint16_t t;

	// Clear TIM4 update interrupt
	TIM_ClearITPendingBit(TIM4, TIM_IT_Update);

	if (ROTUP) t=0xe000; else t=0xe001; //1110 0000 0000 000(0/1)
	State[0]=(State[0]<<1) | t;
	if ((State[0] & 0x003F) == 0b111100) do_ROTUP=1; //Pin be measured low 4 times (0.710ms) and high 2 times (0.350ms) maxes out at 700Hz square wave

	if (ROTDOWN) t=0xe000; else t=0xe001;
	State[1]=(State[1]<<1) | t;
	if ((State[1] & 0x003F) == 0b111100) do_ROTDOWN=1;

	if (LOCK135) t=0xe000; else t=0xe001;
	State[2]=(State[2]<<1) | t;
	if (State[2]==0xfff8) do_LOCK135=1;

	if (LOCK246) t=0xe000; else t=0xe001;
	State[3]=(State[3]<<1) | t;
	if (State[3]==0xfff8) do_LOCK246=1;

}



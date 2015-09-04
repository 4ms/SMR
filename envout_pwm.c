/*
 * envout_pwm.c - PWM output for the channel ENV OUT jacks
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

#include <stm32f4xx.h>
#include "params.h"
#include "globals.h"
#include "dig_inouts.h"
#include "envout_pwm.h"

uint32_t ENVOUT_PWM[NUM_CHANNELS];
float ENVOUT_preload[NUM_CHANNELS];

extern enum Env_Out_Modes env_track_mode;
extern uint32_t env_prepost_mode;
extern float envspeed_attack, envspeed_decay;

extern float channel_level[NUM_CHANNELS];

void init_envout_pwm(void){
	TIM_TimeBaseInitTypeDef tim;
	TIM_OCInitTypeDef  tim_oc;
	GPIO_InitTypeDef gpio;

	int i;

	RCC_APB2PeriphClockCmd(RCC_APB2Periph_TIM1, ENABLE);
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM3, ENABLE);

	gpio.GPIO_Mode = GPIO_Mode_AF;
	gpio.GPIO_Speed = GPIO_Speed_2MHz;
	gpio.GPIO_OType = GPIO_OType_PP;
	gpio.GPIO_PuPd = GPIO_PuPd_UP ;

	RCC_AHB1PeriphClockCmd(ENVOUT_PWM_RCC_12, ENABLE);
	gpio.GPIO_Pin = ENVOUT_PWM_pins_12;
	GPIO_Init(ENVOUT_PWM_GPIO_12, &gpio);

	RCC_AHB1PeriphClockCmd(ENVOUT_PWM_RCC_34, ENABLE);
	gpio.GPIO_Pin = ENVOUT_PWM_pins_34;
	GPIO_Init(ENVOUT_PWM_GPIO_34, &gpio);

	RCC_AHB1PeriphClockCmd(ENVOUT_PWM_RCC_56, ENABLE);
	gpio.GPIO_Pin = ENVOUT_PWM_pins_56;
	GPIO_Init(ENVOUT_PWM_GPIO_56, &gpio);

	GPIO_PinAFConfig(ENVOUT_PWM_GPIO_12, ENVOUT_PWM_pinsource_1, GPIO_AF_TIM3);
	GPIO_PinAFConfig(ENVOUT_PWM_GPIO_12, ENVOUT_PWM_pinsource_2, GPIO_AF_TIM3);
	GPIO_PinAFConfig(ENVOUT_PWM_GPIO_34, ENVOUT_PWM_pinsource_3, GPIO_AF_TIM1);
	GPIO_PinAFConfig(ENVOUT_PWM_GPIO_34, ENVOUT_PWM_pinsource_4, GPIO_AF_TIM1);
	GPIO_PinAFConfig(ENVOUT_PWM_GPIO_56, ENVOUT_PWM_pinsource_5, GPIO_AF_TIM3);
	GPIO_PinAFConfig(ENVOUT_PWM_GPIO_56, ENVOUT_PWM_pinsource_6, GPIO_AF_TIM3);


	for (i=0;i<NUM_CHANNELS;i++)
		ENVOUT_PWM[i]=0;


	TIM_TimeBaseStructInit(&tim);
	tim.TIM_Period = 4096; //168M / 2 / 4096 = 20.5kHz
	tim.TIM_Prescaler = 0;
	tim.TIM_ClockDivision = 0;
	tim.TIM_CounterMode = TIM_CounterMode_Up;
	TIM_TimeBaseInit(TIM3, &tim);
	TIM_TimeBaseInit(TIM1, &tim);

	tim_oc.TIM_OCMode = TIM_OCMode_PWM1;
	tim_oc.TIM_OutputState = TIM_OutputState_Enable;
	tim_oc.TIM_Pulse = 0;
	tim_oc.TIM_OCPolarity = TIM_OCPolarity_High;

	TIM_OC1Init(TIM3, &tim_oc);
	TIM_OC1PreloadConfig(TIM3, TIM_OCPreload_Enable);

	TIM_OC2Init(TIM3, &tim_oc);
	TIM_OC2PreloadConfig(TIM3, TIM_OCPreload_Enable);

	TIM_OC3Init(TIM3, &tim_oc);
	TIM_OC3PreloadConfig(TIM3, TIM_OCPreload_Enable);

	TIM_OC4Init(TIM3, &tim_oc);
	TIM_OC4PreloadConfig(TIM3, TIM_OCPreload_Enable);

	TIM_Cmd(TIM3, ENABLE);

	TIM_OC1Init(TIM1, &tim_oc);
	TIM_OC1PreloadConfig(TIM1, TIM_OCPreload_Enable);

	TIM_OC3Init(TIM1, &tim_oc);
	TIM_OC3PreloadConfig(TIM1, TIM_OCPreload_Enable);

    TIM_CtrlPWMOutputs(TIM1, ENABLE);

	TIM_Cmd(TIM1, ENABLE);
}


//The filter routine sets ENVOUT_preload representing each channel's absolute value output (full-wave rectified)
//update_ENVOUT_PWM() smooths this out with a LPF for attack and decay times
//and converts it to a 12-bit value for feeding into the PWM outputs

void update_ENVOUT_PWM(void){
	uint8_t j;
	static uint32_t env_trigout[NUM_CHANNELS];
	static uint32_t env_low_ctr[NUM_CHANNELS];
	static float envelope[NUM_CHANNELS];

	if (env_track_mode==ENV_SLOW || env_track_mode==ENV_FAST){

		for (j=0;j<NUM_CHANNELS;j++){
			//Apply LPF
			if(envelope[j] < ENVOUT_preload[j]) {
				envelope[j] *= envspeed_attack;
				envelope[j] += (1.0-envspeed_attack)*ENVOUT_preload[j];
			} else {
				envelope[j] *= envspeed_decay;
				envelope[j] += (1.0-envspeed_decay)*ENVOUT_preload[j];
			}

			//Pre-CV (input) or Post faders (quieter)
			//To-Do: Attenuate by a global system parameter
			if (env_prepost_mode==PRE)
				ENVOUT_PWM[j]=((uint32_t)envelope[j])>>18;
			else
				ENVOUT_PWM[j]=(uint32_t)(envelope[j]*channel_level[j])>>16;
		}

	} else { //trigger mode
		for (j=0;j<NUM_CHANNELS;j++){

			//Pre-CV (input) or Post-CV (output)
			if (env_prepost_mode!=PRE) ENVOUT_preload[j]=ENVOUT_preload[j]*channel_level[j];

			if (env_trigout[j]){ //keep the trigger high for about 100ms, ignoring the input signal
				env_trigout[j]--;

			} else {
				if (((uint32_t)ENVOUT_preload[j])>0x02000000) { //about 12.5% of max, or 1V envelope output
					env_low_ctr[j]=0;
					env_trigout[j]=2000; //about 100ms
					ENVOUT_PWM[j]=4090;

				} else {
					if (++env_low_ctr[j]>=2000) //only set the output low if the input signal has stayed low for a bit
						ENVOUT_PWM[j]=0;
				}

			}
		}

	}


	TIM3->ENVOUT_PWM_CC_1=ENVOUT_PWM[0];
	TIM3->ENVOUT_PWM_CC_2=ENVOUT_PWM[1];

	TIM1->ENVOUT_PWM_CC_3=ENVOUT_PWM[2];
	TIM1->ENVOUT_PWM_CC_4=ENVOUT_PWM[3];

	TIM3->ENVOUT_PWM_CC_5=ENVOUT_PWM[4];
	TIM3->ENVOUT_PWM_CC_6=ENVOUT_PWM[5];



}

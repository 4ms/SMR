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

#define VOLTOCT_LUT_MAX 5167
uint32_t voltoct_lut[VOLTOCT_LUT_MAX];

extern enum Env_Out_Modes env_track_mode;
extern uint32_t env_prepost_mode;
extern float envspeed_attack, envspeed_decay;

extern float channel_level[NUM_CHANNELS];

void init_envout_pwm(void){
	TIM_TimeBaseInitTypeDef tim;
	TIM_OCInitTypeDef  tim_oc;
	GPIO_InitTypeDef gpio;
	uint8_t i;

	init_PWM_voltperoctave_lut();

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
	uint32_t k;

	static uint32_t env_trigout[NUM_CHANNELS];
	static uint32_t env_low_ctr[NUM_CHANNELS];
	static float envelope[NUM_CHANNELS];

	if (env_track_mode==ENV_VOLTOCT)
	{
		for (j=0;j<NUM_CHANNELS;j++){
			//8192 sharp
			//8292 220Hz->444Hz
			//8320 444,447
			//8370 460Hz
			k=(uint32_t)(ENVOUT_preload[j] * 8192.0);

			if (k>22) k-=22;
			else k=0;
			if (k>=VOLTOCT_LUT_MAX) k= VOLTOCT_LUT_MAX-1;

			ENVOUT_PWM[j] = voltoct_lut[k];


		//	if (j==0) ENVOUT_PWM[0] = 2559;
		//	if (j==1) ENVOUT_PWM[1] = 2559;
		//	if (j==2) ENVOUT_PWM[2] = 2559;
		//	if (j==3) ENVOUT_PWM[3] = 2559;
		//	if (j==4) ENVOUT_PWM[4] = 2559;
		//	if (j==5) ENVOUT_PWM[5] = 2559;

		}

	}
	else if (env_track_mode==ENV_SLOW || env_track_mode==ENV_FAST)
	{

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

	}
	else
	{ //trigger mode
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


void init_PWM_voltperoctave_lut(void)
{
	uint32_t k,v;

	for (k=0;k<VOLTOCT_LUT_MAX;k++){

		if(k<=0) v=41;
		else if(k<=1) v=83;
		else if(k<=2) v=125;
		else if(k<=3) v=167;
		else if(k<=5) v=209;
		else if(k<=6) v=251;
		else if(k<=8) v=293;
		else if(k<=10) v=335;
		else if(k<=12) v=377;
		else if(k<=14) v=419;
		else if(k<=16) v=461;
		else if(k<=18) v=503;
		else if(k<=21) v=545;
		else if(k<=23) v=587;
		else if(k<=26) v=629;
		else if(k<=29) v=671;
		else if(k<=32) v=713;
		else if(k<=35) v=755;
		else if(k<=38) v=797;
		else if(k<=42) v=839;
		else if(k<=46) v=881;
		else if(k<=50) v=923;
		else if(k<=54) v=965;
		else if(k<=59) v=1007;
		else if(k<=64) v=1049;
		else if(k<=69) v=1091;
		else if(k<=74) v=1133;
		else if(k<=80) v=1175;
		else if(k<=86) v=1217;
		else if(k<=92) v=1259;
		else if(k<=99) v=1301;
		else if(k<=106) v=1343;
		else if(k<=114) v=1385;
		else if(k<=122) v=1427;
		else if(k<=131) v=1469;
		else if(k<=140) v=1511;
		else if(k<=149) v=1553;
		else if(k<=160) v=1595;
		else if(k<=171) v=1637;
		else if(k<=182) v=1679;
		else if(k<=194) v=1721;
		else if(k<=207) v=1763;
		else if(k<=220) v=1805;
		else if(k<=235) v=1847;
		else if(k<=250) v=1889;
		else if(k<=267) v=1931;
		else if(k<=284) v=1973;
		else if(k<=302) v=2015;
		else if(k<=321) v=2057;
		else if(k<=342) v=2099;
		else if(k<=363) v=2141;
		else if(k<=386) v=2183;
		else if(k<=411) v=2225;
		else if(k<=436) v=2267;
		else if(k<=463) v=2309;
		else if(k<=492) v=2351;
		else if(k<=523) v=2393;
		else if(k<=556) v=2435;
		else if(k<=590) v=2477;
		else if(k<=626) v=2519;
		else if(k<=665) v=2561;
		else if(k<=705) v=2603;
		else if(k<=749) v=2645;
		else if(k<=795) v=2687;
		else if(k<=843) v=2729;
		else if(k<=895) v=2771;
		else if(k<=949) v=2813;
		else if(k<=1007) v=2855;
		else if(k<=1069) v=2897;
		else if(k<=1133) v=2939;
		else if(k<=1202) v=2981;
		else if(k<=1275) v=3023;
		else if(k<=1352) v=3065;
		else if(k<=1433) v=3107;
		else if(k<=1520) v=3149;
		else if(k<=1612) v=3191;
		else if(k<=1709) v=3233;
		else if(k<=1812) v=3275;
		else if(k<=1921) v=3317;
		else if(k<=2037) v=3359;
		else if(k<=2159) v=3401;
		else if(k<=2289) v=3443;
		else if(k<=2426) v=3485;
		else if(k<=2572) v=3527;
		else if(k<=2726) v=3569;
		else if(k<=2889) v=3611;
		else if(k<=3063) v=3653;
		else if(k<=3246) v=3695;
		else if(k<=3440) v=3737;
		else if(k<=3646) v=3779;
		else if(k<=3865) v=3821;
		else if(k<=4096) v=3863;
		else if(k<=4340) v=3905;
		else if(k<=4600) v=3947;
		else if(k<=4875) v=3989;
		else if(k<=5166) v=4031;


		voltoct_lut[k]=v;
	}

}

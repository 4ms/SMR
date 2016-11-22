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

float voltoct_pwm_tracking = 1.0f;


void init_envout_pwm(void){
	TIM_TimeBaseInitTypeDef tim;
	TIM_OCInitTypeDef  tim_oc;
	GPIO_InitTypeDef gpio;
	uint8_t i;

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


void init_ENV_update_timer(void)
{
	TIM_TimeBaseInitTypeDef  tim;

	NVIC_InitTypeDef nvic;
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_TIM9, ENABLE);

	nvic.NVIC_IRQChannel = TIM1_BRK_TIM9_IRQn;
	nvic.NVIC_IRQChannelPreemptionPriority = 1;
	nvic.NVIC_IRQChannelSubPriority = 0;
	nvic.NVIC_IRQChannelCmd = ENABLE;
	NVIC_Init(&nvic);

	//168MHz / 30000 ---> 5.6kHz

	TIM_TimeBaseStructInit(&tim);
	tim.TIM_Period = 30000;
	tim.TIM_Prescaler = 0x0;
	tim.TIM_ClockDivision = 0;
	tim.TIM_CounterMode = TIM_CounterMode_Up;

	TIM_TimeBaseInit(TIM9, &tim);

	TIM_ITConfig(TIM9, TIM_IT_Update, ENABLE);

	TIM_Cmd(TIM9, ENABLE);
}

void ENV_update_IRQHandler(void)
{
	if (TIM_GetITStatus(TIM9, TIM_IT_Update) != RESET) {

		update_ENVOUT_PWM();

		TIM_ClearITPendingBit(TIM9, TIM_IT_Update);

	}
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
	uint8_t quantize_voct=0;


	if (env_track_mode==ENV_VOLTOCT) //takes 5us, every 20us
	{
		for (j=0;j<NUM_CHANNELS;j++)
		{
			k=ENVOUT_preload[j] * 8192;		//multiply the preload float value by 8192 so we can do a series of integer comparisons in FreqCoef_to_PWMval()
											//it turns out integer comparisons are faster than float comparisons, and we do a lot of them in FreqCoef_to_PWMval()

			ENVOUT_PWM[j] = (uint32_t)(voltoct_pwm_tracking * (float)(FreqCoef_to_PWMval(k,ENVOUT_preload[j])));
 			ENVOUT_PWM[j] -=165.895; // {3/12th of a volt ((4095/10) * (3/12))} + {0.16 v adjustment = 65.52cnts}
		}
	}
	else if (env_track_mode==ENV_SLOW || env_track_mode==ENV_FAST)
	{

		for (j=0;j<NUM_CHANNELS;j++){
			//Apply LPF
			if(envelope[j] < ENVOUT_preload[j]) {
				envelope[j] *= envspeed_attack;
				envelope[j] += (1.0f-envspeed_attack)*ENVOUT_preload[j];
			} else {
				envelope[j] *= envspeed_decay;
				envelope[j] += (1.0f-envspeed_decay)*ENVOUT_preload[j];
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

uint32_t FreqCoef_to_PWMval(uint32_t k, float v)
{
	float t,b,tval,bval;
	uint32_t result;

	//convert from a .csv:
	// ,$s/\(\d*\),\(\d*\),\(\d*\),\(\d*\)/else if (k<=\1) {t=\1;b=\2;tval=\3;bval=\4;}/
	// ,$s/\(\d*\.\d*\),\(\d*\.\d*\),\(\d*\),\(\d*\)/else if (k<=\1) {t=\1;b=\2;tval=\3;bval=\4;}/

/* This is what the comparisons would be if we did float comparisons.
 * It's more accurate to do it this way, but time tests show that it's slower than comparing integers
 * Todo: Is it possible to optimize float comparisons to be of equal speed as integers?
 *
	if (k<=0.001799870791119) return(0);
	else if (k<=0.001906896677806) {t=0.001906896677806;b=0.001799870791119;tval=43;bval=0;}
	else if (k<=0.002020286654891) {t=0.002020286654891;b=0.001906896677806;tval=85;bval=43;}
	else if (k<=0.002140419150884) {t=0.002140419150884;b=0.002020286654891;tval=128;bval=85;}
	else if (k<=0.002267695096821) {t=0.002267695096821;b=0.002140419150884;tval=171;bval=128;}
	else if (k<=0.002402539264342) {t=0.002402539264342;b=0.002267695096821;tval=213;bval=171;}
	else if (k<=0.002545401683319) {t=0.002545401683319;b=0.002402539264342;tval=256;bval=213;}
	else if (k<=0.002696759143797) {t=0.002696759143797;b=0.002545401683319;tval=299;bval=256;}
	else if (k<=0.002857116787229) {t=0.002857116787229;b=0.002696759143797;tval=341;bval=299;}
	else if (k<=0.003027009792343) {t=0.003027009792343;b=0.002857116787229;tval=384;bval=341;}
	else if (k<=0.003207005161252) {t=0.003207005161252;b=0.003027009792343;tval=427;bval=384;}
	else if (k<=0.003397703611766) {t=0.003397703611766;b=0.003207005161252;tval=469;bval=427;}
	else if (k<=0.003599741582238) {t=0.003599741582238;b=0.003397703611766;tval=512;bval=469;}
	else if (k<=0.003813793355611) {t=0.003813793355611;b=0.003599741582238;tval=555;bval=512;}
	else if (k<=0.004040573309783) {t=0.004040573309783;b=0.003813793355611;tval=597;bval=555;}
	else if (k<=0.004280838301768) {t=0.004280838301768;b=0.004040573309783;tval=640;bval=597;}
	else if (k<=0.004535390193643) {t=0.004535390193643;b=0.004280838301768;tval=683;bval=640;}
	else if (k<=0.004805078528684) {t=0.004805078528684;b=0.004535390193643;tval=725;bval=683;}
	else if (k<=0.005090803366639) {t=0.005090803366639;b=0.004805078528684;tval=768;bval=725;}
	else if (k<=0.005393518287594) {t=0.005393518287594;b=0.005090803366639;tval=811;bval=768;}
	else if (k<=0.005714233574458) {t=0.005714233574458;b=0.005393518287594;tval=853;bval=811;}
	else if (k<=0.006054019584687) {t=0.006054019584687;b=0.005714233574458;tval=896;bval=853;}
	else if (k<=0.006414010322504) {t=0.006414010322504;b=0.006054019584687;tval=939;bval=896;}
	else if (k<=0.006795407223532) {t=0.006795407223532;b=0.006414010322504;tval=981;bval=939;}
	else if (k<=0.007199483164475) {t=0.007199483164475;b=0.006795407223532;tval=1024;bval=981;}
	else if (k<=0.007627586711222) {t=0.007627586711222;b=0.007199483164475;tval=1067;bval=1024;}
	else if (k<=0.008081146619566) {t=0.008081146619566;b=0.007627586711222;tval=1109;bval=1067;}
	else if (k<=0.008561676603536) {t=0.008561676603536;b=0.008081146619566;tval=1152;bval=1109;}
	else if (k<=0.009070780387286) {t=0.009070780387286;b=0.008561676603536;tval=1195;bval=1152;}
	else if (k<=0.009610157057368) {t=0.009610157057368;b=0.009070780387286;tval=1237;bval=1195;}
	else if (k<=0.010181606733277) {t=0.010181606733277;b=0.009610157057368;tval=1280;bval=1237;}
	else if (k<=0.010787036575188) {t=0.010787036575188;b=0.010181606733277;tval=1323;bval=1280;}
	else if (k<=0.011428467148915) {t=0.011428467148915;b=0.010787036575188;tval=1365;bval=1323;}
	else if (k<=0.012108039169373) {t=0.012108039169373;b=0.011428467148915;tval=1408;bval=1365;}
	else if (k<=0.012828020645008) {t=0.012828020645008;b=0.012108039169373;tval=1451;bval=1408;}
	else if (k<=0.013590814447065) {t=0.013590814447065;b=0.012828020645008;tval=1493;bval=1451;}
	else if (k<=0.01439896632895) {t=0.01439896632895;b=0.013590814447065;tval=1536;bval=1493;}
	else if (k<=0.015255173422445) {t=0.015255173422445;b=0.01439896632895;tval=1579;bval=1536;}
	else if (k<=0.016162293239131) {t=0.016162293239131;b=0.015255173422445;tval=1621;bval=1579;}
	else if (k<=0.017123353207072) {t=0.017123353207072;b=0.016162293239131;tval=1664;bval=1621;}
	else if (k<=0.018141560774572) {t=0.018141560774572;b=0.017123353207072;tval=1707;bval=1664;}
	else if (k<=0.019220314114735) {t=0.019220314114735;b=0.018141560774572;tval=1749;bval=1707;}
	else if (k<=0.020363213466555) {t=0.020363213466555;b=0.019220314114735;tval=1792;bval=1749;}
	else if (k<=0.021574073150375) {t=0.021574073150375;b=0.020363213466555;tval=1835;bval=1792;}
	else if (k<=0.02285693429783) {t=0.02285693429783;b=0.021574073150375;tval=1877;bval=1835;}
	else if (k<=0.024216078338746) {t=0.024216078338746;b=0.02285693429783;tval=1920;bval=1877;}
	else if (k<=0.025656041290015) {t=0.025656041290015;b=0.024216078338746;tval=1963;bval=1920;}
	else if (k<=0.027181628894129) {t=0.027181628894129;b=0.025656041290015;tval=2005;bval=1963;}
	else if (k<=0.0287979326579) {t=0.0287979326579;b=0.027181628894129;tval=2048;bval=2005;}
	else if (k<=0.03051034684489) {t=0.03051034684489;b=0.0287979326579;tval=2091;bval=2048;}
	else if (k<=0.032324586478262) {t=0.032324586478262;b=0.03051034684489;tval=2133;bval=2091;}
	else if (k<=0.034246706414144) {t=0.034246706414144;b=0.032324586478262;tval=2176;bval=2133;}
	else if (k<=0.036283121549144) {t=0.036283121549144;b=0.034246706414144;tval=2219;bval=2176;}
	else if (k<=0.03844062822947) {t=0.03844062822947;b=0.036283121549144;tval=2261;bval=2219;}
	else if (k<=0.04072642693311) {t=0.04072642693311;b=0.03844062822947;tval=2304;bval=2261;}
	else if (k<=0.04314814630075) {t=0.04314814630075;b=0.04072642693311;tval=2347;bval=2304;}
	else if (k<=0.04571386859566) {t=0.04571386859566;b=0.04314814630075;tval=2389;bval=2347;}
	else if (k<=0.048432156677492) {t=0.048432156677492;b=0.04571386859566;tval=2432;bval=2389;}
	else if (k<=0.05131208258003) {t=0.05131208258003;b=0.048432156677492;tval=2475;bval=2432;}
	else if (k<=0.054363257788259) {t=0.054363257788259;b=0.05131208258003;tval=2517;bval=2475;}
	else if (k<=0.057595865315801) {t=0.057595865315801;b=0.054363257788259;tval=2560;bval=2517;}
	else if (k<=0.061020693689779) {t=0.061020693689779;b=0.057595865315801;tval=2603;bval=2560;}
	else if (k<=0.064649172956524) {t=0.064649172956524;b=0.061020693689779;tval=2645;bval=2603;}
	else if (k<=0.068493412828289) {t=0.068493412828289;b=0.064649172956524;tval=2688;bval=2645;}
	else if (k<=0.072566243098287) {t=0.072566243098287;b=0.068493412828289;tval=2731;bval=2688;}
	else if (k<=0.07688125645894) {t=0.07688125645894;b=0.072566243098287;tval=2773;bval=2731;}
	else if (k<=0.081452853866219) {t=0.081452853866219;b=0.07688125645894;tval=2816;bval=2773;}
	else if (k<=0.086296292601501) {t=0.086296292601501;b=0.081452853866219;tval=2859;bval=2816;}
	else if (k<=0.091427737191321) {t=0.091427737191321;b=0.086296292601501;tval=2901;bval=2859;}
	else if (k<=0.096864313354985) {t=0.096864313354985;b=0.091427737191321;tval=2944;bval=2901;}
	else if (k<=0.102624165160061) {t=0.102624165160061;b=0.096864313354985;tval=2987;bval=2944;}
	else if (k<=0.108726515576518) {t=0.108726515576518;b=0.102624165160061;tval=3029;bval=2987;}
	else if (k<=0.115191730631602) {t=0.115191730631602;b=0.108726515576518;tval=3072;bval=3029;}
	else if (k<=0.122041387379559) {t=0.122041387379559;b=0.115191730631602;tval=3115;bval=3072;}
	else if (k<=0.129298345913049) {t=0.129298345913049;b=0.122041387379559;tval=3157;bval=3115;}
	else if (k<=0.136986825656577) {t=0.136986825656577;b=0.129298345913049;tval=3200;bval=3157;}
	else if (k<=0.145132486196575) {t=0.145132486196575;b=0.136986825656577;tval=3243;bval=3200;}
	else if (k<=0.153762512917881) {t=0.153762512917881;b=0.145132486196575;tval=3285;bval=3243;}
	else if (k<=0.162905707732438) {t=0.162905707732438;b=0.153762512917881;tval=3328;bval=3285;}
	else if (k<=0.172592585203002) {t=0.172592585203002;b=0.162905707732438;tval=3371;bval=3328;}
	else if (k<=0.182855474382642) {t=0.182855474382642;b=0.172592585203002;tval=3413;bval=3371;}
	else if (k<=0.19372862670997) {t=0.19372862670997;b=0.182855474382642;tval=3456;bval=3413;}
	else if (k<=0.205248330320122) {t=0.205248330320122;b=0.19372862670997;tval=3499;bval=3456;}
	else if (k<=0.217453031153035) {t=0.217453031153035;b=0.205248330320122;tval=3541;bval=3499;}
	else if (k<=0.230383461263203) {t=0.230383461263203;b=0.217453031153035;tval=3584;bval=3541;}
	else if (k<=0.244082774759117) {t=0.244082774759117;b=0.230383461263203;tval=3627;bval=3584;}
	else if (k<=0.258596691826098) {t=0.258596691826098;b=0.244082774759117;tval=3669;bval=3627;}
	else if (k<=0.273973651313155) {t=0.273973651313155;b=0.258596691826098;tval=3712;bval=3669;}
	else if (k<=0.290264972393149) {t=0.290264972393149;b=0.273973651313155;tval=3755;bval=3712;}
	else if (k<=0.307525025835761) {t=0.307525025835761;b=0.290264972393149;tval=3797;bval=3755;}
	else if (k<=0.325811415464877) {t=0.325811415464877;b=0.307525025835761;tval=3840;bval=3797;}
	else if (k<=0.345185170406003) {t=0.345185170406003;b=0.325811415464877;tval=3883;bval=3840;}
	else if (k<=0.365710948765283) {t=0.365710948765283;b=0.345185170406003;tval=3925;bval=3883;}
	else if (k<=0.387457253419939) {t=0.387457253419939;b=0.365710948765283;tval=3968;bval=3925;}
	else if (k<=0.410496660640243) {t=0.410496660640243;b=0.387457253419939;tval=4011;bval=3968;}
	else if (k<=0.43490606230607) {t=0.43490606230607;b=0.410496660640243;tval=4053;bval=4011;}
	else if (k<=0.460766922526406) {t=0.460766922526406;b=0.43490606230607;tval=4096;bval=4053;}
	else return(4095);
*/

	if (k<=14) return(0);
	else if (k<=15) {t=0.001906896677806;b=0.001799870791119;tval=43;bval=0;}
	else if (k<=16) {t=0.002020286654891;b=0.001906896677806;tval=85;bval=43;}
	else if (k<=17) {t=0.002140419150884;b=0.002020286654891;tval=128;bval=85;}
	else if (k<=18) {t=0.002267695096821;b=0.002140419150884;tval=171;bval=128;}
	else if (k<=19) {t=0.002402539264342;b=0.002267695096821;tval=213;bval=171;}
	else if (k<=20) {t=0.002545401683319;b=0.002402539264342;tval=256;bval=213;}
	else if (k<=22) {t=0.002696759143797;b=0.002545401683319;tval=299;bval=256;}
	else if (k<=23) {t=0.002857116787229;b=0.002696759143797;tval=341;bval=299;}
	else if (k<=24) {t=0.003027009792343;b=0.002857116787229;tval=384;bval=341;}
	else if (k<=26) {t=0.003207005161252;b=0.003027009792343;tval=427;bval=384;}
	else if (k<=27) {t=0.003397703611766;b=0.003207005161252;tval=469;bval=427;}
	else if (k<=29) {t=0.003599741582238;b=0.003397703611766;tval=512;bval=469;}
	else if (k<=31) {t=0.003813793355611;b=0.003599741582238;tval=555;bval=512;}
	else if (k<=33) {t=0.004040573309783;b=0.003813793355611;tval=597;bval=555;}
	else if (k<=35) {t=0.004280838301768;b=0.004040573309783;tval=640;bval=597;}
	else if (k<=37) {t=0.004535390193643;b=0.004280838301768;tval=683;bval=640;}
	else if (k<=39) {t=0.004805078528684;b=0.004535390193643;tval=725;bval=683;}
	else if (k<=41) {t=0.005090803366639;b=0.004805078528684;tval=768;bval=725;}
	else if (k<=44) {t=0.005393518287594;b=0.005090803366639;tval=811;bval=768;}
	else if (k<=46) {t=0.005714233574458;b=0.005393518287594;tval=853;bval=811;}
	else if (k<=49) {t=0.006054019584687;b=0.005714233574458;tval=896;bval=853;}
	else if (k<=52) {t=0.006414010322504;b=0.006054019584687;tval=939;bval=896;}
	else if (k<=55) {t=0.006795407223532;b=0.006414010322504;tval=981;bval=939;}
	else if (k<=58) {t=0.007199483164475;b=0.006795407223532;tval=1024;bval=981;}
	else if (k<=62) {t=0.007627586711222;b=0.007199483164475;tval=1067;bval=1024;}
	else if (k<=66) {t=0.008081146619566;b=0.007627586711222;tval=1109;bval=1067;}
	else if (k<=70) {t=0.008561676603536;b=0.008081146619566;tval=1152;bval=1109;}
	else if (k<=74) {t=0.009070780387286;b=0.008561676603536;tval=1195;bval=1152;}
	else if (k<=78) {t=0.009610157057368;b=0.009070780387286;tval=1237;bval=1195;}
	else if (k<=83) {t=0.010181606733277;b=0.009610157057368;tval=1280;bval=1237;}
	else if (k<=88) {t=0.010787036575188;b=0.010181606733277;tval=1323;bval=1280;}
	else if (k<=93) {t=0.011428467148915;b=0.010787036575188;tval=1365;bval=1323;}
	else if (k<=99) {t=0.012108039169373;b=0.011428467148915;tval=1408;bval=1365;}
	else if (k<=105) {t=0.012828020645008;b=0.012108039169373;tval=1451;bval=1408;}
	else if (k<=111) {t=0.013590814447065;b=0.012828020645008;tval=1493;bval=1451;}
	else if (k<=117) {t=0.01439896632895;b=0.013590814447065;tval=1536;bval=1493;}
	else if (k<=124) {t=0.015255173422445;b=0.01439896632895;tval=1579;bval=1536;}
	else if (k<=132) {t=0.016162293239131;b=0.015255173422445;tval=1621;bval=1579;}
	else if (k<=140) {t=0.017123353207072;b=0.016162293239131;tval=1664;bval=1621;}
	else if (k<=148) {t=0.018141560774572;b=0.017123353207072;tval=1707;bval=1664;}
	else if (k<=157) {t=0.019220314114735;b=0.018141560774572;tval=1749;bval=1707;}
	else if (k<=166) {t=0.020363213466555;b=0.019220314114735;tval=1792;bval=1749;}
	else if (k<=176) {t=0.021574073150375;b=0.020363213466555;tval=1835;bval=1792;}
	else if (k<=187) {t=0.02285693429783;b=0.021574073150375;tval=1877;bval=1835;}
	else if (k<=198) {t=0.024216078338746;b=0.02285693429783;tval=1920;bval=1877;}
	else if (k<=210) {t=0.025656041290015;b=0.024216078338746;tval=1963;bval=1920;}
	else if (k<=222) {t=0.027181628894129;b=0.025656041290015;tval=2005;bval=1963;}
	else if (k<=235) {t=0.0287979326579;b=0.027181628894129;tval=2048;bval=2005;}
	else if (k<=249) {t=0.03051034684489;b=0.0287979326579;tval=2091;bval=2048;}
	else if (k<=264) {t=0.032324586478262;b=0.03051034684489;tval=2133;bval=2091;}
	else if (k<=280) {t=0.034246706414144;b=0.032324586478262;tval=2176;bval=2133;}
	else if (k<=297) {t=0.036283121549144;b=0.034246706414144;tval=2219;bval=2176;}
	else if (k<=314) {t=0.03844062822947;b=0.036283121549144;tval=2261;bval=2219;}
	else if (k<=333) {t=0.04072642693311;b=0.03844062822947;tval=2304;bval=2261;}
	else if (k<=353) {t=0.04314814630075;b=0.04072642693311;tval=2347;bval=2304;}
	else if (k<=374) {t=0.04571386859566;b=0.04314814630075;tval=2389;bval=2347;}
	else if (k<=396) {t=0.048432156677492;b=0.04571386859566;tval=2432;bval=2389;}
	else if (k<=420) {t=0.05131208258003;b=0.048432156677492;tval=2475;bval=2432;}
	else if (k<=445) {t=0.054363257788259;b=0.05131208258003;tval=2517;bval=2475;}
	else if (k<=471) {t=0.057595865315801;b=0.054363257788259;tval=2560;bval=2517;}
	else if (k<=499) {t=0.061020693689779;b=0.057595865315801;tval=2603;bval=2560;}
	else if (k<=529) {t=0.064649172956524;b=0.061020693689779;tval=2645;bval=2603;}
	else if (k<=561) {t=0.068493412828289;b=0.064649172956524;tval=2688;bval=2645;}
	else if (k<=594) {t=0.072566243098287;b=0.068493412828289;tval=2731;bval=2688;}
	else if (k<=629) {t=0.07688125645894;b=0.072566243098287;tval=2773;bval=2731;}
	else if (k<=667) {t=0.081452853866219;b=0.07688125645894;tval=2816;bval=2773;}
	else if (k<=706) {t=0.086296292601501;b=0.081452853866219;tval=2859;bval=2816;}
	else if (k<=748) {t=0.091427737191321;b=0.086296292601501;tval=2901;bval=2859;}
	else if (k<=793) {t=0.096864313354985;b=0.091427737191321;tval=2944;bval=2901;}
	else if (k<=840) {t=0.102624165160061;b=0.096864313354985;tval=2987;bval=2944;}
	else if (k<=890) {t=0.108726515576518;b=0.102624165160061;tval=3029;bval=2987;}
	else if (k<=943) {t=0.115191730631602;b=0.108726515576518;tval=3072;bval=3029;}
	else if (k<=999) {t=0.122041387379559;b=0.115191730631602;tval=3115;bval=3072;}
	else if (k<=1059) {t=0.129298345913049;b=0.122041387379559;tval=3157;bval=3115;}
	else if (k<=1122) {t=0.136986825656577;b=0.129298345913049;tval=3200;bval=3157;}
	else if (k<=1188) {t=0.145132486196575;b=0.136986825656577;tval=3243;bval=3200;}
	else if (k<=1259) {t=0.153762512917881;b=0.145132486196575;tval=3285;bval=3243;}
	else if (k<=1334) {t=0.162905707732438;b=0.153762512917881;tval=3328;bval=3285;}
	else if (k<=1413) {t=0.172592585203002;b=0.162905707732438;tval=3371;bval=3328;}
	else if (k<=1497) {t=0.182855474382642;b=0.172592585203002;tval=3413;bval=3371;}
	else if (k<=1587) {t=0.19372862670997;b=0.182855474382642;tval=3456;bval=3413;}
	else if (k<=1681) {t=0.205248330320122;b=0.19372862670997;tval=3499;bval=3456;}
	else if (k<=1781) {t=0.217453031153035;b=0.205248330320122;tval=3541;bval=3499;}
	else if (k<=1887) {t=0.230383461263203;b=0.217453031153035;tval=3584;bval=3541;}
	else if (k<=1999) {t=0.244082774759117;b=0.230383461263203;tval=3627;bval=3584;}
	else if (k<=2118) {t=0.258596691826098;b=0.244082774759117;tval=3669;bval=3627;}
	else if (k<=2244) {t=0.273973651313155;b=0.258596691826098;tval=3712;bval=3669;}
	else if (k<=2377) {t=0.290264972393149;b=0.273973651313155;tval=3755;bval=3712;}
	else if (k<=2519) {t=0.307525025835761;b=0.290264972393149;tval=3797;bval=3755;}
	else if (k<=2669) {t=0.325811415464877;b=0.307525025835761;tval=3840;bval=3797;}
	else if (k<=2827) {t=0.345185170406003;b=0.325811415464877;tval=3883;bval=3840;}
	else if (k<=2995) {t=0.365710948765283;b=0.345185170406003;tval=3925;bval=3883;}
	else if (k<=3174) {t=0.387457253419939;b=0.365710948765283;tval=3968;bval=3925;}
	else if (k<=3362) {t=0.410496660640243;b=0.387457253419939;tval=4011;bval=3968;}
	else if (k<=3562) {t=0.43490606230607;b=0.410496660640243;tval=4053;bval=4011;}
	else if (k<=3774) {t=0.460766922526406;b=0.43490606230607;tval=4096;bval=4053;}
	else return(4095);

	result = ( ((t-v)/(t-b))*bval + ((v-b)/(t-b))*tval );
	
	asm("usat %[dst], #12, %[src]" : [dst] "=r" (result) : [src] "r" (result));

	return(result);

}

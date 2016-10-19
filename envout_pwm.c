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
//#include "PWM_Voct_UNQ.h"

uint32_t ENVOUT_PWM[NUM_CHANNELS];
float ENVOUT_preload[NUM_CHANNELS];

#define VOLTOCT_LUT_MAX 5167
uint32_t voltoct_lut[VOLTOCT_LUT_MAX];

extern enum Env_Out_Modes env_track_mode;
extern uint32_t env_prepost_mode;
extern float envspeed_attack, envspeed_decay;

extern float channel_level[NUM_CHANNELS];

//extern uint32_t PWM_Voct_UNQ[21353];

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
	uint8_t quantize_voct=0;

	if (env_track_mode==ENV_VOLTOCT)
	{

		for (j=0;j<NUM_CHANNELS;j++){

				k=(uint32_t)(ENVOUT_preload[j] * 8192.0);

				if (k>22) k-=22;
				else k=0;
				if (k>=VOLTOCT_LUT_MAX)
					k= VOLTOCT_LUT_MAX-1;

				if (quantize_voct)
					ENVOUT_PWM[j] = voltoct_lut[k];
				else
					ENVOUT_PWM[j] = PWM_UNQ(k);

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


void init_PWM_voltperoctave_lut(void)
{
	uint32_t k,v;

	//Qunatized table
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

uint32_t PWM_UNQ(uint32_t k)
{
	float t,b,tval,bval;
	uint32_t result;

//Todo: try using uint32_t math instead of floats, and compare efficiency

	//convert from a .csv:
	//:,$s/\(\d*\),\(\d*\),\(\d*\),\(\d*\)/else if (k<=\1) {t=\1;b=\2;tval=\3;bval=\4;}/

	DEBUGA_ON(DEBUG0);

	if (k==0) return(42);
	else if (k<=1) {t=1;b=0;tval=84;bval=42;}
	else if (k<=2) {t=2;b=1;tval=127;bval=84;}
	else if (k<=3) {t=3;b=2;tval=169;bval=127;}
	else if (k<=5) {t=5;b=3;tval=212;bval=169;}
	else if (k<=6) {t=6;b=5;tval=255;bval=212;}
	else if (k<=8) {t=8;b=6;tval=297;bval=255;}
	else if (k<=10) {t=10;b=8;tval=340;bval=297;}
	else if (k<=12) {t=12;b=10;tval=382;bval=340;}
	else if (k<=14) {t=14;b=12;tval=425;bval=382;}
	else if (k<=16) {t=16;b=14;tval=467;bval=425;}
	else if (k<=18) {t=18;b=16;tval=510;bval=467;}
	else if (k<=21) {t=21;b=18;tval=553;bval=510;}
	else if (k<=23) {t=23;b=21;tval=595;bval=553;}
	else if (k<=26) {t=26;b=23;tval=638;bval=595;}
	else if (k<=29) {t=29;b=26;tval=680;bval=638;}
	else if (k<=32) {t=32;b=29;tval=723;bval=680;}
	else if (k<=35) {t=35;b=32;tval=766;bval=723;}
	else if (k<=38) {t=38;b=35;tval=808;bval=766;}
	else if (k<=42) {t=42;b=38;tval=851;bval=808;}
	else if (k<=46) {t=46;b=42;tval=893;bval=851;}
	else if (k<=50) {t=50;b=46;tval=936;bval=893;}
	else if (k<=54) {t=54;b=50;tval=978;bval=936;}
	else if (k<=59) {t=59;b=54;tval=1021;bval=978;}
	else if (k<=64) {t=64;b=59;tval=1064;bval=1021;}
	else if (k<=69) {t=69;b=64;tval=1106;bval=1064;}
	else if (k<=74) {t=74;b=69;tval=1149;bval=1106;}
	else if (k<=80) {t=80;b=74;tval=1191;bval=1149;}
	else if (k<=86) {t=86;b=80;tval=1234;bval=1191;}
	else if (k<=92) {t=92;b=86;tval=1277;bval=1234;}
	else if (k<=99) {t=99;b=92;tval=1319;bval=1277;}
	else if (k<=106) {t=106;b=99;tval=1362;bval=1319;}
	else if (k<=114) {t=114;b=106;tval=1404;bval=1362;}
	else if (k<=122) {t=122;b=114;tval=1447;bval=1404;}
	else if (k<=131) {t=131;b=122;tval=1489;bval=1447;}
	else if (k<=140) {t=140;b=131;tval=1532;bval=1489;}
	else if (k<=149) {t=149;b=140;tval=1575;bval=1532;}
	else if (k<=160) {t=160;b=149;tval=1617;bval=1575;}
	else if (k<=171) {t=171;b=160;tval=1660;bval=1617;}
	else if (k<=182) {t=182;b=171;tval=1702;bval=1660;}
	else if (k<=194) {t=194;b=182;tval=1745;bval=1702;}
	else if (k<=207) {t=207;b=194;tval=1788;bval=1745;}
	else if (k<=220) {t=220;b=207;tval=1830;bval=1788;}
	else if (k<=235) {t=235;b=220;tval=1873;bval=1830;}
	else if (k<=250) {t=250;b=235;tval=1915;bval=1873;}
	else if (k<=267) {t=267;b=250;tval=1958;bval=1915;}
	else if (k<=284) {t=284;b=267;tval=2000;bval=1958;}
	else if (k<=302) {t=302;b=284;tval=2043;bval=2000;}
	else if (k<=321) {t=321;b=302;tval=2086;bval=2043;}
	else if (k<=342) {t=342;b=321;tval=2128;bval=2086;}
	else if (k<=363) {t=363;b=342;tval=2171;bval=2128;}
	else if (k<=386) {t=386;b=363;tval=2213;bval=2171;}
	else if (k<=411) {t=411;b=386;tval=2256;bval=2213;}
	else if (k<=436) {t=436;b=411;tval=2299;bval=2256;}
	else if (k<=463) {t=463;b=436;tval=2341;bval=2299;}
	else if (k<=492) {t=492;b=463;tval=2384;bval=2341;}
	else if (k<=523) {t=523;b=492;tval=2426;bval=2384;}
	else if (k<=556) {t=556;b=523;tval=2469;bval=2426;}
	else if (k<=590) {t=590;b=556;tval=2511;bval=2469;}
	else if (k<=626) {t=626;b=590;tval=2554;bval=2511;}
	else if (k<=665) {t=665;b=626;tval=2597;bval=2554;}
	else if (k<=705) {t=705;b=665;tval=2639;bval=2597;}
	else if (k<=749) {t=749;b=705;tval=2682;bval=2639;}
	else if (k<=795) {t=795;b=749;tval=2724;bval=2682;}
	else if (k<=843) {t=843;b=795;tval=2767;bval=2724;}
	else if (k<=895) {t=895;b=843;tval=2810;bval=2767;}
	else if (k<=949) {t=949;b=895;tval=2852;bval=2810;}
	else if (k<=1007) {t=1007;b=949;tval=2895;bval=2852;}
	else if (k<=1069) {t=1069;b=1007;tval=2937;bval=2895;}
	else if (k<=1133) {t=1133;b=1069;tval=2980;bval=2937;}
	else if (k<=1202) {t=1202;b=1133;tval=3022;bval=2980;}
	else if (k<=1275) {t=1275;b=1202;tval=3065;bval=3022;}
	else if (k<=1352) {t=1352;b=1275;tval=3108;bval=3065;}
	else if (k<=1433) {t=1433;b=1352;tval=3150;bval=3108;}
	else if (k<=1520) {t=1520;b=1433;tval=3193;bval=3150;}
	else if (k<=1612) {t=1612;b=1520;tval=3235;bval=3193;}
	else if (k<=1709) {t=1709;b=1612;tval=3278;bval=3235;}
	else if (k<=1812) {t=1812;b=1709;tval=3321;bval=3278;}
	else if (k<=1921) {t=1921;b=1812;tval=3363;bval=3321;}
	else if (k<=2037) {t=2037;b=1921;tval=3406;bval=3363;}
	else if (k<=2159) {t=2159;b=2037;tval=3448;bval=3406;}
	else if (k<=2289) {t=2289;b=2159;tval=3491;bval=3448;}
	else if (k<=2426) {t=2426;b=2289;tval=3533;bval=3491;}
	else if (k<=2572) {t=2572;b=2426;tval=3576;bval=3533;}
	else if (k<=2726) {t=2726;b=2572;tval=3619;bval=3576;}
	else if (k<=2889) {t=2889;b=2726;tval=3661;bval=3619;}
	else if (k<=3063) {t=3063;b=2889;tval=3704;bval=3661;}
	else if (k<=3246) {t=3246;b=3063;tval=3746;bval=3704;}
	else if (k<=3440) {t=3440;b=3246;tval=3789;bval=3746;}
	else if (k<=3646) {t=3646;b=3440;tval=3832;bval=3789;}
	else if (k<=3865) {t=3865;b=3646;tval=3874;bval=3832;}
	else if (k<=4096) {t=4096;b=3865;tval=3917;bval=3874;}
	else if (k<=4340) {t=4340;b=4096;tval=3959;bval=3917;}
	else if (k<=4600) {t=4600;b=4340;tval=4002;bval=3959;}
	else if (k<=4875) {t=4875;b=4600;tval=4044;bval=4002;}
	else if (k<=5166) {t=5166;b=4875;tval=4087;bval=4044;}

	result = ( ((t-k)/(t-b))*bval + ((k-b)/(t-b))*tval );

	DEBUGA_OFF(DEBUG0);
	return(result);

}

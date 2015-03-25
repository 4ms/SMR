/*
 * inouts.c
 */

//#include <stm32f4xx_gpio.h>
//#include <stm32f4xx_rcc.h>
#include "inouts.h"

#include "globals.h"

uint16_t mod_mode_135;
uint16_t mod_mode_246;
uint16_t rotate_to_next_scale;

uint8_t do_ROTDOWN;
uint8_t do_ROTUP;
uint8_t do_LOCK135;
uint8_t do_LOCK246;

float envspeed_attack, envspeed_decay;


extern float adc_lag_attack;
extern float adc_lag_decay;
uint32_t env_prepost_mode;
extern __IO uint16_t adc_buffer[NUM_ADCS];


const int LED_LOCK[6]={LED_LOCK1, LED_LOCK2, LED_LOCK3, LED_LOCK4, LED_LOCK5, LED_LOCK6};
inline void LOCKLED_ON(int led){
	LED_GPIO->BSRRL = LED_LOCK[led];
}
inline void LOCKLED_OFF(int led){
	LED_GPIO->BSRRH = LED_LOCK[led];
}

const int _lockbutton[6]={LOCK1_pin, LOCK2_pin, LOCK3_pin, LOCK4_pin, LOCK5_pin, LOCK6_pin};
inline uint8_t LOCKBUTTON(uint8_t x){
	return (!(LOCKBUT_GPIO->IDR & _lockbutton[x]));
}

void init_inouts(void){
	GPIO_InitTypeDef gpio;
	int i;


	// Set up Outputs:

	RCC_AHB1PeriphClockCmd(LED_RCC, ENABLE);

	GPIO_StructInit(&gpio);
	gpio.GPIO_Pin = LED_CLIP1 | LED_CLIP2 | LED_CLIP3 | LED_CLIP4 | LED_LOCK1 | LED_LOCK6 | LED_CLIP5 | LED_CLIP6 | LED_RING_OE;
	gpio.GPIO_Mode = GPIO_Mode_OUT;
	gpio.GPIO_Speed = GPIO_Speed_50MHz;
	gpio.GPIO_OType = GPIO_OType_PP;
	gpio.GPIO_PuPd = GPIO_PuPd_NOPULL;
	GPIO_Init(LED_GPIO, &gpio);

	gpio.GPIO_Pin = LED_LOCK1 | LED_LOCK2 | LED_LOCK3 | LED_LOCK4 | LED_LOCK5 | LED_LOCK6;
	GPIO_Init(LED_GPIO, &gpio);


	RCC_AHB1PeriphClockCmd(DEBUGA_RCC, ENABLE);
	gpio.GPIO_Pin = DEBUG0 | DEBUG1 | DEBUG2;
	gpio.GPIO_Mode = GPIO_Mode_OUT;
	gpio.GPIO_Speed = GPIO_Speed_50MHz;
	gpio.GPIO_OType = GPIO_OType_PP;
	gpio.GPIO_PuPd = GPIO_PuPd_NOPULL;
	GPIO_Init(DEBUGA_GPIO, &gpio);

	RCC_AHB1PeriphClockCmd(DEBUGB_RCC, ENABLE);
	gpio.GPIO_Pin = DEBUG3;
	GPIO_Init(DEBUGB_GPIO, &gpio);

	RCC_AHB1PeriphClockCmd(DEBUGC_RCC, ENABLE);
	gpio.GPIO_Pin = DEBUG4;
	GPIO_Init(DEBUGC_GPIO, &gpio);

	//Set up Inputs:

	RCC_AHB1PeriphClockCmd(BUTTON_RCC, ENABLE);
	RCC_AHB1PeriphClockCmd(BUTTON2_RCC, ENABLE);
	RCC_AHB1PeriphClockCmd(ROT_RCC, ENABLE);


	gpio.GPIO_Mode = GPIO_Mode_IN;
	gpio.GPIO_Speed = GPIO_Speed_25MHz;
	gpio.GPIO_PuPd = GPIO_PuPd_UP;

	gpio.GPIO_Pin = ENV_MODE_pin;
	GPIO_Init(ENV_MODE_GPIO, &gpio);

	gpio.GPIO_Pin = ENVSPEEDSLOW_pin;
	GPIO_Init(ENVSPEEDSLOW_GPIO, &gpio);

	gpio.GPIO_Pin = ENVSPEEDFAST_pin;
	GPIO_Init(ENVSPEEDFAST_GPIO, &gpio);

	gpio.GPIO_Pin = LOCK1_pin | LOCK2_pin | LOCK3_pin | LOCK4_pin | LOCK5_pin | LOCK6_pin;
	GPIO_Init(LOCKBUT_GPIO, &gpio);

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

TIM_TimeBaseInitTypeDef  tim;
void init_inputread_timer(void){
	  uint16_t PrescalerValue = 0;

	  NVIC_InitTypeDef nvic;

	  RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM4, ENABLE);

	  nvic.NVIC_IRQChannel = TIM4_IRQn;
	  nvic.NVIC_IRQChannelPreemptionPriority = 0;
	  nvic.NVIC_IRQChannelSubPriority = 1;
	  nvic.NVIC_IRQChannelCmd = ENABLE;
	  NVIC_Init(&nvic);

	  TIM_TimeBaseStructInit(&tim);
	  tim.TIM_Period = 65535;
	  tim.TIM_Prescaler = 0;
	  tim.TIM_ClockDivision = 0;
	  tim.TIM_CounterMode = TIM_CounterMode_Up;

	  TIM_TimeBaseInit(TIM4, &tim);

	  TIM_ITConfig(TIM4, TIM_IT_Update, ENABLE);

	  TIM_Cmd(TIM4, ENABLE);

}

uint16_t State[6] = {0,0,0,0,0,0}; // Current debounce status


void TIM4_IRQHandler(void)
{
	uint16_t t;

	// Clear TIM4 update interrupt
	TIM_ClearITPendingBit(TIM4, TIM_IT_Update);

	if (ROTUP) t=0xe000; else t=0xe001; //1110 0000 0000 000(0/1)
	State[0]=(State[0]<<1) | t;
	if (State[0]==0xf000) do_ROTUP=1; //1111 0000 0000 0000

	if (ROTDOWN) t=0xe000; else t=0xe001;
	State[1]=(State[1]<<1) | t;
	if (State[1]==0xf000) do_ROTDOWN=1;

	if (LOCK135) t=0xe000; else t=0xe001;
	State[2]=(State[2]<<1) | t;
	if (State[2]==0xf000) do_LOCK135=1;

	if (LOCK246) t=0xe000; else t=0xe001;
	State[3]=(State[3]<<1) | t;
	if (State[3]==0xf000) do_LOCK246=1;

}

void poll_switches(void){
	static uint32_t old_cvlag=0xFFFF;
	uint32_t lag_val;
	float t_adc_lag_decay, t_adc_lag_attack;


/*** Read Switches ***/

		//PRE|POST switch
		if (ENV_MODE){
			env_prepost_mode=0;
		} else {
			env_prepost_mode=1;
		}

		if (MOD246){
			mod_mode_246=6;
		} else {
			mod_mode_246=246;
		}

		if (MOD135){
			mod_mode_135=1;
		} else {
			mod_mode_135=135;
		}

		if (RANGE_MODE){
			rotate_to_next_scale=1;
		} else {
			rotate_to_next_scale=0;
		}

		//float ga = exp(-1.0f/(SampleRate*AttackTimeInSecond));
		if (ENVSPEEDSLOW) {
			envspeed_attack=0.999170;
			envspeed_decay=0.999170;
		} else {
			if (ENVSPEEDFAST){
				envspeed_attack=0.999896;
				envspeed_decay=0.999896;
			} else {
				envspeed_attack=0.999896;
				envspeed_decay=0.999170;
			}
		}



		lag_val=CVLAG;
		if (old_cvlag!=lag_val){
			old_cvlag=lag_val;

			if (lag_val){
				lag_val=adc_buffer[MORPH_ADC];
				if (lag_val<200) lag_val=200;

				t_adc_lag_attack=	1.0 - (1.0/((lag_val)*0.1));
				t_adc_lag_decay=	1.0 - (1.0/((lag_val)*0.25));

				if (t_adc_lag_attack<0)	adc_lag_attack=0;
				else adc_lag_attack = t_adc_lag_attack;
				if (t_adc_lag_decay<0)	adc_lag_decay=0;
				else adc_lag_decay = t_adc_lag_decay;

			}else{
				adc_lag_attack=0;
				adc_lag_decay=0;
			}
		}

}


/*
short read_rotary_fullstep(void){
        static short greycode=0;

        if ((greycode==0b11) && !ROTROTARY_CW){ //greycode was 0b11, now it's 0b01 so rotary went down
                greycode=0b01;
                return(1);
        }

        else if ((greycode==0b11) && !ROTROTARY_CCW){ //greycode was 0b11, now it's 0b10 so rotary went up
                greycode=0b10;
                return(-1);
        }

        else if (ROTROTARY_CW && ROTROTARY_CCW) //greycode is 11
                greycode=0b11;

        else greycode=0b00;

        return(0);
}
*/


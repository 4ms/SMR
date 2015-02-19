/*
 * envout_pwm.c

 *
 *  Created on: Nov 16, 2014
 *      Author: design
 */
#include "stm32f4xx.h"
#include "stm32f4xx_tim.h"

#include "globals.h"
#include "inouts.h"
#include "envout_pwm.h"

extern uint32_t ENVOUT_PWM[NUM_ACTIVE_FILTS];

void init_envout_pwm(void){
	TIM_TimeBaseInitTypeDef tim;
	TIM_OCInitTypeDef  tim_oc;
	GPIO_InitTypeDef gpio;

	int i;


	RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM3, ENABLE);

	RCC_AHB1PeriphClockCmd(ENVOUT_PWM_RCC_1256, ENABLE);
	gpio.GPIO_Pin = ENVOUT_PWM_pins_1256;
	gpio.GPIO_Mode = GPIO_Mode_AF;
	gpio.GPIO_Speed = GPIO_Speed_50MHz;
	gpio.GPIO_OType = GPIO_OType_PP;
	gpio.GPIO_PuPd = GPIO_PuPd_UP ;
	GPIO_Init(ENVOUT_PWM_GPIO_1256, &gpio);

	//Turn the mis-connected pins PB2 and PB3 to inputs (only needed for a fix for pcb p4)
	gpio.GPIO_Pin = GPIO_Pin_2 | GPIO_Pin_3;
	gpio.GPIO_Mode = GPIO_Mode_IN;
	gpio.GPIO_PuPd = GPIO_PuPd_NOPULL;
	GPIO_Init(GPIOB, &gpio);

	RCC_APB2PeriphClockCmd(RCC_APB2Periph_TIM1, ENABLE);
	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA, ENABLE);
	gpio.GPIO_Pin = (GPIO_Pin_8 | GPIO_Pin_10);
	gpio.GPIO_Mode = GPIO_Mode_AF;
	gpio.GPIO_Speed = GPIO_Speed_50MHz;
	gpio.GPIO_OType = GPIO_OType_PP;
	gpio.GPIO_PuPd = GPIO_PuPd_UP ;
	GPIO_Init(GPIOA, &gpio);



	GPIO_PinAFConfig(ENVOUT_PWM_GPIO_1256, ENVOUT_PWM_pinsource_1, GPIO_AF_TIM3);
	GPIO_PinAFConfig(ENVOUT_PWM_GPIO_1256, ENVOUT_PWM_pinsource_2, GPIO_AF_TIM3);
	GPIO_PinAFConfig(ENVOUT_PWM_GPIO_1256, ENVOUT_PWM_pinsource_5, GPIO_AF_TIM3);
	GPIO_PinAFConfig(ENVOUT_PWM_GPIO_1256, ENVOUT_PWM_pinsource_6, GPIO_AF_TIM3);

	GPIO_PinAFConfig(GPIOA, GPIO_PinSource8, GPIO_AF_TIM1);
	GPIO_PinAFConfig(GPIOA, GPIO_PinSource10, GPIO_AF_TIM1);

	for (i=0;i<NUM_ACTIVE_FILTS;i++)
		ENVOUT_PWM[i]=0;


	TIM_TimeBaseStructInit(&tim);
	tim.TIM_Period = 4096;
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



void update_ENVOUT_PWM(void){
	TIM3->CCR3=ENVOUT_PWM[0]; //PB0
	TIM3->CCR4=ENVOUT_PWM[1]; //PB1

	TIM3->CCR1=ENVOUT_PWM[4]; //PB4
	TIM3->CCR2=ENVOUT_PWM[5]; //PB5

	TIM1->CCR1=ENVOUT_PWM[2]; //PA8
	TIM1->CCR3=ENVOUT_PWM[3]; //PA10


}

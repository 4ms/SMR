/*
 * inouts.c
 */

#include <stm32f4xx_gpio.h>
#include <stm32f4xx_rcc.h>
#include "globals.h"
#include "inouts.h"



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

void init_inouts(void){
	GPIO_InitTypeDef gpio;
	int i;


	// Set up Outputs:

	RCC_AHB1PeriphClockCmd(LED_RCC, ENABLE);

	GPIO_StructInit(&gpio);
	gpio.GPIO_Pin = LED_CLIP1 | LED_CLIP2 | LED_CLIP3 | LED_CLIP4 | LED_LOCK0 | LED_LOCK5 | LED_CLIP5 | LED_CLIP6 | LED_RING_OE;
	gpio.GPIO_Mode = GPIO_Mode_OUT;
	gpio.GPIO_Speed = GPIO_Speed_50MHz;
	gpio.GPIO_OType = GPIO_OType_PP;
	gpio.GPIO_PuPd = GPIO_PuPd_NOPULL;
	GPIO_Init(LED_GPIO, &gpio);

	RCC_AHB1PeriphClockCmd(DEBUG_RCC, ENABLE);

	gpio.GPIO_Pin = DEBUG0 | DEBUG1 | DEBUG2 | DEBUG3;
	gpio.GPIO_Mode = GPIO_Mode_OUT;
	gpio.GPIO_Speed = GPIO_Speed_50MHz;
	gpio.GPIO_OType = GPIO_OType_PP;
	gpio.GPIO_PuPd = GPIO_PuPd_NOPULL;
	GPIO_Init(DEBUG_GPIO, &gpio);

	//Set up Inputs:

	RCC_AHB1PeriphClockCmd(BUTTON_RCC, ENABLE);

	gpio.GPIO_Pin = ENV_MODE_pin;
	gpio.GPIO_Mode = GPIO_Mode_IN;
	gpio.GPIO_Speed = GPIO_Speed_50MHz;
	gpio.GPIO_PuPd = GPIO_PuPd_UP;
	GPIO_Init(ENV_MODE_GPIO, &gpio);

	gpio.GPIO_Pin = ENVSPEED_pin;
	gpio.GPIO_Mode = GPIO_Mode_IN;
	gpio.GPIO_Speed = GPIO_Speed_50MHz;
	gpio.GPIO_PuPd = GPIO_PuPd_UP;
	GPIO_Init(ENVSPEED_GPIO, &gpio);

//	gpio.GPIO_Pin = SPREAD_TRIG_pin;
//	GPIO_Init(SPREAD_TRIG_GPIO, &gpio);


	gpio.GPIO_Pin = LOCK0_pin;
	GPIO_Init(LOCK0_GPIO, &gpio);

	gpio.GPIO_Pin = LOCK5_pin;
	GPIO_Init(LOCK5_GPIO, &gpio);

	gpio.GPIO_Pin = ROTUP_pin;
	GPIO_Init(ROTUP_GPIO, &gpio);

	gpio.GPIO_Pin = ROTDOWN_pin;
	GPIO_Init(ROTDOWN_GPIO, &gpio);

	gpio.GPIO_Pin = CVLAG_pin;
	gpio.GPIO_PuPd = GPIO_PuPd_UP;
	GPIO_Init(CVLAG_GPIO, &gpio);


}

/*
 * rotary.c
 *
 *  Created on: Jan 13, 2015
 *      Author: design
 */
#include "stm32f4xx.h"
#include "rotary.h"

#define R_START 0x0

#ifdef HALF_STEP
// Use the half-step state table (emits a code at 00 and 11)
#define R_CCW_BEGIN 0x1
#define R_CW_BEGIN 0x2
#define R_START_M 0x3
#define R_CW_BEGIN_M 0x4
#define R_CCW_BEGIN_M 0x5
const unsigned char ttable[6][4] = {
  // R_START (00)
  {R_START_M,            R_CW_BEGIN,     R_CCW_BEGIN,  R_START},
  // R_CCW_BEGIN
  {R_START_M | DIR_CCW, R_START,        R_CCW_BEGIN,  R_START},
  // R_CW_BEGIN
  {R_START_M | DIR_CW,  R_CW_BEGIN,     R_START,      R_START},
  // R_START_M (11)
  {R_START_M,            R_CCW_BEGIN_M,  R_CW_BEGIN_M, R_START},
  // R_CW_BEGIN_M
  {R_START_M,            R_START_M,      R_CW_BEGIN_M, R_START | DIR_CW},
  // R_CCW_BEGIN_M
  {R_START_M,            R_CCW_BEGIN_M,  R_START_M,    R_START | DIR_CCW},
};
#else
// Use the full-step state table (emits a code at 00 only)
#define R_CW_FINAL 0x1
#define R_CW_BEGIN 0x2
#define R_CW_NEXT 0x3
#define R_CCW_BEGIN 0x4
#define R_CCW_FINAL 0x5
#define R_CCW_NEXT 0x6

const unsigned char ttable[7][4] = {
  // R_START
  {R_START,    R_CW_BEGIN,  R_CCW_BEGIN, R_START},
  // R_CW_FINAL
  {R_CW_NEXT,  R_START,     R_CW_FINAL,  R_START | DIR_CW},
  // R_CW_BEGIN
  {R_CW_NEXT,  R_CW_BEGIN,  R_START,     R_START},
  // R_CW_NEXT
  {R_CW_NEXT,  R_CW_BEGIN,  R_CW_FINAL,  R_START},
  // R_CCW_BEGIN
  {R_CCW_NEXT, R_START,     R_CCW_BEGIN, R_START},
  // R_CCW_FINAL
  {R_CCW_NEXT, R_CCW_FINAL, R_START,     R_START | DIR_CCW},
  // R_CCW_NEXT
  {R_CCW_NEXT, R_CCW_FINAL, R_CCW_BEGIN, R_START},
};
#endif

unsigned char state;

void init_rotary(void){
	GPIO_InitTypeDef gpio;

	RCC_AHB1PeriphClockCmd(ROTARY_RCC, ENABLE);

	gpio.GPIO_Mode = GPIO_Mode_IN;
	gpio.GPIO_Speed = GPIO_Speed_50MHz;
	gpio.GPIO_PuPd = GPIO_PuPd_UP;
	gpio.GPIO_Pin = (ROTARY_A_pin | ROTARY_B_pin | ROTARY_SW_pin);
	GPIO_Init(ROTARY_GPIO, &gpio);

	state=R_START;

}

uint8_t read_rotary(void){

	  unsigned char pinstate = ((ROTARY_A?1:0) << 1) | (ROTARY_B?1:0);
	  // Determine new state from the pins and state table.
	  state = ttable[state & 0xf][pinstate];
	  // Return emit bits, ie the generated event.
	  return state & 0x30;

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

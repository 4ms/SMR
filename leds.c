/*
 * leds.c - handles interfacing the slider LEDs and Lock button LEDs
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

#include "leds.h"
#include "params.h"
#include "globals.h"
#include "dig_inouts.h"
#include "system_mode.h"

extern uint8_t q_locked[NUM_CHANNELS];
extern enum UI_Modes ui_mode;
extern float channel_level[NUM_CHANNELS];
extern uint8_t 	cur_param_bank;
extern uint8_t lock[NUM_CHANNELS];
extern uint8_t editscale_notelocked;

uint8_t slider_led_mode=SHOW_CLIPPING;

const uint32_t slider_led[6]={LED_SLIDER1, LED_SLIDER2, LED_SLIDER3, LED_SLIDER4, LED_SLIDER5, LED_SLIDER6};

#define NITERIDER_SPEED 1000

void update_slider_LEDs(void){
	static float f_slider_pwm=0;
	static uint16_t flash=0;
	static uint8_t ready_to_go_on[NUM_CHANNELS];
	uint8_t i;

	if (ui_mode==EDIT_COLORS || ui_mode==PRE_EDIT_COLORS){

		if (flash++>1000){
			LED_SLIDER_ON(slider_led[0] | slider_led[1] | slider_led[2]);
		}
		if (flash>2000){
			flash=0;
			LED_SLIDER_OFF(slider_led[0] | slider_led[1] | slider_led[2]);
		}

		LED_SLIDER_OFF(slider_led[3] | slider_led[4] | slider_led[5]);

	} else if (ui_mode==SELECT_PARAMS || ui_mode==PRE_SELECT_PARAMS){

		if (slider_led_mode==SHOW_CLIPPING){
			if (flash++>NITERIDER_SPEED*10) flash=0;
			if (flash==NITERIDER_SPEED*9) {LED_SLIDER_ON(slider_led[1]);LED_SLIDER_OFF(slider_led[2]);}
			if (flash==NITERIDER_SPEED*8) {LED_SLIDER_ON(slider_led[2]);LED_SLIDER_OFF(slider_led[3]);}
			if (flash==NITERIDER_SPEED*7) {LED_SLIDER_ON(slider_led[3]);LED_SLIDER_OFF(slider_led[4]);}
			if (flash==NITERIDER_SPEED*6) {LED_SLIDER_ON(slider_led[4]);LED_SLIDER_OFF(slider_led[5]);}

			if (flash==NITERIDER_SPEED*5) {LED_SLIDER_ON(slider_led[5]);LED_SLIDER_OFF(slider_led[4]);}
			if (flash==NITERIDER_SPEED*4) {LED_SLIDER_ON(slider_led[4]);LED_SLIDER_OFF(slider_led[3]);}
			if (flash==NITERIDER_SPEED*3) {LED_SLIDER_ON(slider_led[3]);LED_SLIDER_OFF(slider_led[2]);}
			if (flash==NITERIDER_SPEED*2) {LED_SLIDER_ON(slider_led[2]);LED_SLIDER_OFF(slider_led[1]);}
			if (flash==NITERIDER_SPEED*1) {LED_SLIDER_ON(slider_led[1]);LED_SLIDER_OFF(slider_led[0]);}
			if (flash==NITERIDER_SPEED*0) {LED_SLIDER_ON(slider_led[0]);LED_SLIDER_OFF(slider_led[1]);}
		} else {
			if (flash++>NITERIDER_SPEED*10) flash=0;
			if (flash==NITERIDER_SPEED*9) {LED_SLIDER_OFF(slider_led[1]);LED_SLIDER_ON(slider_led[2]);}
			if (flash==NITERIDER_SPEED*8) {LED_SLIDER_OFF(slider_led[2]);LED_SLIDER_ON(slider_led[3]);}
			if (flash==NITERIDER_SPEED*7) {LED_SLIDER_OFF(slider_led[3]);LED_SLIDER_ON(slider_led[4]);}
			if (flash==NITERIDER_SPEED*6) {LED_SLIDER_OFF(slider_led[4]);LED_SLIDER_ON(slider_led[5]);}

			if (flash==NITERIDER_SPEED*5) {LED_SLIDER_OFF(slider_led[5]);LED_SLIDER_ON(slider_led[4]);}
			if (flash==NITERIDER_SPEED*4) {LED_SLIDER_OFF(slider_led[4]);LED_SLIDER_ON(slider_led[3]);}
			if (flash==NITERIDER_SPEED*3) {LED_SLIDER_OFF(slider_led[3]);LED_SLIDER_ON(slider_led[2]);}
			if (flash==NITERIDER_SPEED*2) {LED_SLIDER_OFF(slider_led[2]);LED_SLIDER_ON(slider_led[1]);}
			if (flash==NITERIDER_SPEED*1) {LED_SLIDER_OFF(slider_led[1]);LED_SLIDER_ON(slider_led[0]);}
			if (flash==NITERIDER_SPEED*0) {LED_SLIDER_OFF(slider_led[0]);LED_SLIDER_ON(slider_led[1]);}

		}

	} else {

		// f_slider_pwm is the PWM counter for slider LED brightness

		f_slider_pwm-=0.08; 		//8.0/0.08 = 100 steps
		if (f_slider_pwm<=0.0) {
			f_slider_pwm=4.0;		 //1.0/4.0 = 25% PWM max brigtness

			for (i=0;i<NUM_CHANNELS;i++){
				LED_SLIDER_OFF((slider_led[i]));
				ready_to_go_on[i]=1;
			}

		}
		for (i=0;i<NUM_CHANNELS;i++){

			// Display channel_level[] as an analog value on the LEDs using PWM
			if (channel_level[i]>=f_slider_pwm && ready_to_go_on[i]){
				LED_SLIDER_ON((slider_led[i]));
				ready_to_go_on[i]=0;
			}
		}
	}
}

inline void update_lock_leds(void){
	uint8_t i;
	static uint32_t flash1=0;
	static uint32_t flash2=0;
	static uint32_t lock_led_update_ctr=0;

	if (ui_mode==SELECT_PARAMS || ui_mode==PRE_SELECT_PARAMS){
		if (++flash1>9000) flash1=0;
		if (++flash2>16000) flash2=0;

		for (i=0;i<NUM_CHANNELS;i++){
			if (cur_param_bank==i){
				LOCKLED_ON(i);
			}
			else if (is_bank_filled(i)){
				if (flash2>4000) LOCKLED_ON(i);
				else LOCKLED_OFF(i);
			}
			else {
				if (flash1>8000) LOCKLED_ON(i);
				else LOCKLED_OFF(i);
			}
		}
	} else if (ui_mode==EDIT_COLORS || ui_mode==PRE_EDIT_COLORS){
		for (i=0;i<NUM_CHANNELS;i++){
			if (lock[i]) LOCKLED_ON(i);
			else LOCKLED_OFF(i);
		}

	} else if (ui_mode==EDIT_SCALES){
		for (i=0;i<NUM_CHANNELS;i++){
			if (editscale_notelocked) LOCKLED_ON(i);
			else LOCKLED_OFF(i);
		}

	} else {
		if (lock_led_update_ctr++>QLOCK_FLASH_SPEED){
			lock_led_update_ctr=0;

			if (++flash1>=16) flash1=0;

			for (i=0;i<NUM_CHANNELS;i++){

				if (q_locked[i] && !flash1){
					if (lock[i]) LOCKLED_OFF(i);
					else LOCKLED_ON(i);
				} else {
					if (lock[i]) LOCKLED_ON(i);
					else LOCKLED_OFF(i);
				}
			}
		}
	}
}

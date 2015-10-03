/*
 * led_ring.c - handles interfacing the RGB LED ring
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
#include "led_ring.h"
#include "pca9685_driver.h"
#include "adc.h"
#include "params.h"
#include "globals.h"
#include "dig_inouts.h"
#include "system_mode.h"


//#define TEST_LED_RING


extern float channel_level[NUM_CHANNELS];
extern uint8_t lock[NUM_CHANNELS];

extern float motion_morphpos[NUM_CHANNELS];
extern int8_t motion_fadeto_note[NUM_CHANNELS];
extern int8_t motion_scale_dest[NUM_CHANNELS];

extern uint8_t note[NUM_CHANNELS];
extern uint8_t scale_bank[NUM_CHANNELS];
extern uint8_t hover_scale_bank;
extern int16_t change_scale_mode;

extern uint32_t ENVOUT_PWM[NUM_CHANNELS];
extern enum UI_Modes ui_mode;

uint8_t flag_update_LED_ring=0;
float spectral_readout[NUM_FILTS];



//Default values, that should be overwritten when reading flash
uint8_t cur_colsch=0;

float COLOR_CH[16][6][3];

const float DEFAULT_COLOR_CH[16][6][3]={
		{{0, 0, 761}, {0, 770, 766}, {0, 766, 16}, {700, 700, 700}, {763, 154, 0}, {766, 0, 112}},
		{{0, 0, 761}, {0, 780, 766}, {768, 767, 764}, {493, 768, 2}, {763, 154, 0}, {580, 65, 112}},
		{{767, 0, 0}, {767, 28, 386}, {0, 0, 764}, {0, 320, 387}, {767, 768, 1}, {767, 774, 765}},
		{{0, 0, 761}, {106, 0, 508}, {762, 769, 764}, {0, 767, 1}, {706, 697, 1}, {765, 179, 1}},
		{{0,0,900}, {200,200,816}, {800,800,800},	{900,400,800},	{900,500,202}, {800,200,0}},
		{{0,0,766}, {766,150,0}, {0,50,766}, {766,100,0}, {0,150,766}, {766,50,0}},
		{{0,0,1000}, {0,100,766}, {0,200,666}, {0,300,500}, {0,350,500}, {0,350,400}},

		{{895, 663, 1023}, {895, 663, 1023}, {895, 663, 1023}, {895, 663, 1023}, {895, 663, 1023}, {895, 663, 1023}},
		{{1023, 1, 1023}, {1023, 0, 602}, {1021, 0, 217}, {1023, 0, 118}, {1023, 0, 105}, {1023, 4, 65}},
		{{1023, 1, 0}, {1023, 33, 0}, {1023, 139, 0}, {1023, 275, 0}, {1023, 446, 0}, {1023, 698, 0}},
		{{0, 1023, 0}, {0, 1022, 0}, {1, 1020, 4}, {0, 1023, 164}, {1, 1023, 86}, {0, 1023, 94}},
		{{612, 37, 0}, {614, 0, 203}, {126, 0, 898}, {0, 1023, 461}, {894, 1023, 0}, {941, 996, 1000}},
		{{1021, 1, 0}, {1021, 1, 0}, {1021, 1, 0}, {1021, 1, 0}, {1021, 1, 0}, {1021, 1, 0}},
		{{1023, 1023, 0}, {1022, 1021, 1}, {1021, 1022, 32}, {1023, 1023, 110}, {1023, 1023, 164}, {1023, 1023, 222}},

		{{100,100,100},{100,100,100},{100,100,100},{100,100,100},{100,100,100},{100,100,100}},
		{{100,100,100},{100,100,100},{100,100,100},{100,100,100},{100,100,100},{100,100,100}}
};


const float SCALE_BANK_COLOR[20][3]={
//const float SCALE_BANK_COLOR[NUMSCALEBANKS][3]={

		{800,800,800}, 		//white: western 1 interval
		{0,1000,0},			//green: indian pentatonic
		{200,200,816},			//lavendar: alpha sp2
		{0,800,800},		//cyan: alpha sp1
		{766,50,0},		//burnt orange: gamma sp1
		{800,800,0},		//yellow: 17ET
		{800,0,800},		//pink: twelve tone
		{600,0,200},		//orange: diatonic1
		{600,0,60},			//rose: diatonic2

		{300,1000,300},		//light green: western two interval
		{1000,0,0},			//red: mesopotamian
		{766,500,30},		//yellow-green: shrutis

		{0,0,816},			//blue: B296
		{0,200,500},		//skyblue : gamelan
		{600,1000,0},		//lime : bohlen-pierce

		{100,100,100}, 		//pearl: user

		{100,100,100}, 		//pearl: user
		{100,100,100}, 		//pearl: user
		{100,100,100} 		//pearl: user

};

const float USER_SCALE_BANK[3] = {50,50,50};

void set_default_color_scheme(void){
	uint32_t i=1152;
	uint8_t *src;
	uint8_t *dst;

	src = (uint8_t *)DEFAULT_COLOR_CH;
	dst = (uint8_t *)COLOR_CH;

	while (i--)
		*dst++ = *src++;

}

void calculate_envout_leds(uint16_t env_out_leds[NUM_CHANNELS][3]){
	uint8_t chan=0;
	uint8_t fw_ctr;

	if (ui_mode==SELECT_PARAMS || ui_mode==EDIT_COLORS || ui_mode==PRE_SELECT_PARAMS || ui_mode==PRE_EDIT_COLORS){
		for (chan=0;chan<6;chan++){
			env_out_leds[chan][0]=(uint16_t)((COLOR_CH[cur_colsch][chan][0])  );
			env_out_leds[chan][1]=(uint16_t)((COLOR_CH[cur_colsch][chan][1])  );
			env_out_leds[chan][2]=(uint16_t)((COLOR_CH[cur_colsch][chan][2])  );
		}
	} else if (ui_mode==EDIT_SCALES) {
		for (chan=0;chan<6;chan++){
			env_out_leds[chan][0]=(FW_VERSION & (1<<fw_ctr++)) ? 500 : 0;
			env_out_leds[chan][1]=(FW_VERSION & (1<<fw_ctr++)) ? 500 : 0;
			env_out_leds[chan][2]=(FW_VERSION & (1<<fw_ctr++)) ? 500 : 0;
		}
	} else {
		for (chan=0;chan<6;chan++){
			env_out_leds[chan][0]=(uint16_t)((COLOR_CH[cur_colsch][chan][0]) * ( (float)ENVOUT_PWM[chan]/4096.0) );
			env_out_leds[chan][1]=(uint16_t)((COLOR_CH[cur_colsch][chan][1]) * ( (float)ENVOUT_PWM[chan]/4096.0) );
			env_out_leds[chan][2]=(uint16_t)((COLOR_CH[cur_colsch][chan][2]) * ( (float)ENVOUT_PWM[chan]/4096.0) );
		}
	}

}

void display_filter_rotation(void){
	uint16_t ring[20][3];
	uint16_t env_out_leds[6][3];
	uint8_t i, next_i,chan=0;
	float inv_fade[6],fade[6];
	float t_f;
	static uint8_t flash=0;

	uint16_t ring_a[20][3];
	uint16_t ring_b[20][3];

#ifdef TEST_LED_RING
	for (chan=0;chan<20;chan++){
		ring[chan][0]=512;
		ring[chan][1]=512;
		ring[chan][2]=512;
	}

	for (chan=0;chan<6;chan++){
		env_out_leds[chan][0]=512;
		env_out_leds[chan][1]=512;
		env_out_leds[chan][2]=512;
	}


#else

	for (i=0;i<20;i++){
		ring[i][0]=0;
		ring[i][1]=0;
		ring[i][2]=0;
	}

	// Set the brightness of each LED in the ring:
	// --if it's unlocked, then brightness corresponds to the slider+cv level. Keep a minimum of 5% so that it doesn't go totally off
	// --if it's locked, brightness flashes between 100% and 0%.
	// As we rotate morph between two LEDs in the ring:
	// --fade[chan] is the brightness of the end point LED
	// --inv_fade[chan] is the brightness of the start point LED
	if (ui_mode==EDIT_COLORS) flash=1;
	else flash=1-flash;

	for (chan=0;chan<6;chan++){
		if (ui_mode==EDIT_COLORS) t_f=1.0;
		else if (ui_mode==EDIT_SCALES) t_f = (chan==0) ? 1.0 : 0.0;
		else
			t_f = channel_level[chan] < 0.05 ? 0.05 : channel_level[chan];

		if (lock[chan]==0){
			inv_fade[chan] = (1.0-motion_morphpos[chan])*(t_f);
			fade[chan] = motion_morphpos[chan]*(t_f);

		} else {
			fade[chan]=0.0;
			if (flash) inv_fade[chan]=t_f;
			else inv_fade[chan]=0.0;
		}
	}

	for (i=0;i<20;i++){
		for (chan=0;chan<6;chan++){
			next_i=motion_fadeto_note[chan];
			if (note[chan]==i){

				if (inv_fade[chan]>0.0){
					if (ring[i][0]+ring[i][1]+ring[i][2]==0){
						ring[i][0] = (uint16_t)((COLOR_CH[cur_colsch][chan][0])*inv_fade[chan]);
						ring[i][1] = (uint16_t)((COLOR_CH[cur_colsch][chan][1])*inv_fade[chan]);
						ring[i][2] = (uint16_t)((COLOR_CH[cur_colsch][chan][2])*inv_fade[chan]);
					}else {
						ring[i][0] += (uint16_t)((COLOR_CH[cur_colsch][chan][0])*inv_fade[chan]);
						ring[i][1] += (uint16_t)((COLOR_CH[cur_colsch][chan][1])*inv_fade[chan]);
						ring[i][2] += (uint16_t)((COLOR_CH[cur_colsch][chan][2])*inv_fade[chan]);

						if (ring[i][0]>1023) ring[i][0]=1023;
						if (ring[i][1]>1023) ring[i][1]=1023;
						if (ring[i][2]>1023) ring[i][2]=1023;
					}
				}
				if (fade[chan]>0.0){
					if (ring[next_i][0]+ring[next_i][1]+ring[next_i][2]==0){
						ring[next_i][0]=(uint16_t)((COLOR_CH[cur_colsch][chan][0])*fade[chan]);
						ring[next_i][1]=(uint16_t)((COLOR_CH[cur_colsch][chan][1])*fade[chan]);
						ring[next_i][2]=(uint16_t)((COLOR_CH[cur_colsch][chan][2])*fade[chan]);
					} else {
						ring[next_i][0]+=(uint16_t)((COLOR_CH[cur_colsch][chan][0])*fade[chan]);
						ring[next_i][1]+=(uint16_t)((COLOR_CH[cur_colsch][chan][1])*fade[chan]);
						ring[next_i][2]+=(uint16_t)((COLOR_CH[cur_colsch][chan][2])*fade[chan]);

						if (ring[next_i][0]>1023) ring[next_i][0]=1023;
						if (ring[next_i][1]>1023) ring[next_i][1]=1023;
						if (ring[next_i][2]>1023) ring[next_i][2]=1023;

					}
				}
				chan=6;//break;
			}
		}
	}
	calculate_envout_leds(env_out_leds);
#endif

	LEDDriver_set_LED_ring(ring, env_out_leds);
}



void display_scale(void){
	//There's probably a more efficient way of calculating this!
	uint16_t ring[20][3];
	uint8_t i,j, chan;
	static uint8_t flash=0;
	uint16_t env_out_leds[6][3];
	float inv_fade[6],fade[6];

	uint8_t elacs[NUMSCALES][NUM_CHANNELS];
	uint8_t elacs_num[NUMSCALES];
	static uint8_t elacs_ctr[NUMSCALES]={0,0,0,0,0,0,0,0,0,0,0};

	if (flash++>3) flash=0;

	//Destination of fade:

	// --Blank out the reverse-hash scale table
	for (i=0;i<NUMSCALES;i++) {
		elacs_num[i]=0;
		elacs[i][0]=99;
		elacs[i][1]=99;
		elacs[i][2]=99;
		elacs[i][3]=99;
		elacs[i][4]=99;
		elacs[i][5]=99;
	}

	// --Each entry in elacs[][] equals the number of channels
	for (i=0;i<NUM_CHANNELS;i++){
		elacs[motion_scale_dest[i]][elacs_num[motion_scale_dest[i]]] = i;
		elacs_num[motion_scale_dest[i]]++;
	}

	for (i=0;i<NUMSCALES;i++) {
		j=(i+(20-NUMSCALES/2)) % 20; //ring positions 15 16 17 18 19 0 1 2 3 4 5

		if (flash==0) {
			elacs_ctr[i]++;
			if (elacs_ctr[i] >= elacs_num[i]) elacs_ctr[i]=0;
		}

		// --Blank out the channel if there are no entries
		if (elacs[i][0]==99){
			ring[j][0]=15;
			ring[j][1]=15;
			ring[j][2]=5;


		} else {
			ring[j][0]=COLOR_CH[cur_colsch][  elacs[i][ elacs_ctr[i] ]  ][0];
			ring[j][1]=COLOR_CH[cur_colsch][  elacs[i][ elacs_ctr[i] ]  ][1];
			ring[j][2]=COLOR_CH[cur_colsch][  elacs[i][ elacs_ctr[i] ]  ][2];

		}
	}

	// --Show the scale bank settings
	for (i=0;i<NUM_CHANNELS;i++){
		j=13-i; //13, 12, 11, 10, 9, 8

		if (lock[i]!=1) {
			ring[j][0]=SCALE_BANK_COLOR[hover_scale_bank][0];
			ring[j][1]=SCALE_BANK_COLOR[hover_scale_bank][1];
			ring[j][2]=SCALE_BANK_COLOR[hover_scale_bank][2];
		} else
		if (scale_bank[i]==0xFF){
			ring[j][0]=USER_SCALE_BANK[0];
			ring[j][1]=USER_SCALE_BANK[1];
			ring[j][2]=USER_SCALE_BANK[2];
		}
		else {
			ring[j][0]=SCALE_BANK_COLOR[scale_bank[i]][0];
			ring[j][1]=SCALE_BANK_COLOR[scale_bank[i]][1];
			ring[j][2]=SCALE_BANK_COLOR[scale_bank[i]][2];
		}
	}

	// --Blank out three spots to separate scale and bank
	//6
	ring[NUMSCALES/2+1][0]=0;
	ring[NUMSCALES/2+1][1]=0;
	ring[NUMSCALES/2+1][2]=0;

	//7
	ring[13-NUM_CHANNELS][0]=0;
	ring[13-NUM_CHANNELS][1]=0;
	ring[13-NUM_CHANNELS][2]=0;

	//14
	ring[19-NUMSCALES/2][0]=0;
	ring[19-NUMSCALES/2][1]=0;
	ring[19-NUMSCALES/2][2]=0;

	calculate_envout_leds(env_out_leds);

	LEDDriver_set_LED_ring(ring, env_out_leds);
}

//Not used, but could be useful!
void display_spectral_readout(void){

	uint16_t ring[20][3];
	uint16_t env_out_leds[6][3];

	uint8_t i,chan;
	uint16_t t;
	uint32_t t32;

	for (i=0;i<20;i++){

		t32=((uint32_t)spectral_readout[i])>>15;
		if (t32>1024) t=1023;
		else if (t32<300) t=0;
		else t=(uint16_t)t32;

		ring[i][0]=t;
		ring[i][1]=t;
		ring[i][2]=t;

	}

	calculate_envout_leds(env_out_leds);

	LEDDriver_set_LED_ring(ring, env_out_leds);

}


inline void update_LED_ring(void){

	static uint32_t led_ring_update_ctr=0;

	if (led_ring_update_ctr++>2000 || flag_update_LED_ring){
		led_ring_update_ctr=0;
		flag_update_LED_ring=0;

		if (change_scale_mode){
			display_scale();
		} else {
			//if (display_spec) display_spectral_readout();
			//else
			display_filter_rotation();
		}

	}


}




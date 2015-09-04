/*
 * user_scales.c - Handles UI for user-inputted scales
 *
 * Author: Dan Green (danngreen1@gmail.com)
 * 2015
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



#include "globals.h"
#include "params.h"
#include "dig_inouts.h"
#include "rotary.h"
#include "system_mode.h"
#include "user_scales.h"

extern float exp_1voct[4096];

float user_scalebank[231];

float DEFAULT_user_scalebank[21]={
		0.02094395102393198,
		0.0221893431599245,
		0.02350879016601388,
		0.02490669557392844,
		0.02638772476301919,
		0.02795682053052971,
		0.02961921958772248,
		0.03138047003691591,
		0.03324644988776009,
		0.03522338667454755,
		0.03731787824003011,
		0.03953691475510571,
		0.04188790204786397,
		0.04437868631984903,
		0.04701758033202778,
		0.0498133911478569,
		0.0527754495260384,
		0.05591364106105944,
		0.05923843917544499,
		0.06276094007383184,
		0.06649289977552018
};


uint8_t editscale_notelocked=0;

extern uint8_t note[NUM_CHANNELS];
extern uint8_t scale[NUM_CHANNELS];
extern uint8_t scale_bank[NUM_CHANNELS];
extern uint8_t lock[NUM_CHANNELS];

extern enum UI_Modes ui_mode;


extern __IO uint16_t adc_buffer[NUM_ADCS];
extern __IO uint16_t potadc_buffer[NUM_ADC3S];

extern int16_t change_scale_mode;

uint8_t old_scale_bank[NUM_CHANNELS];
uint8_t old_note[NUM_CHANNELS];
uint8_t old_scale[NUM_CHANNELS];

void enter_edit_scale(void){
	uint8_t i;
	for (i=0;i<NUM_CHANNELS;i++) {
		old_scale_bank[i]=scale_bank[i];
		old_scale[i]=scale[i];
		old_note[i]=note[i];

		scale_bank[i]=NUMSCALEBANKS-1;
	}

	editscale_notelocked=1;

	ui_mode=EDIT_SCALES;
}

void exit_edit_scale(uint8_t save){
	uint8_t i;
	for (i=0;i<NUM_CHANNELS;i++) {
		scale_bank[i]=old_scale_bank[i];
		scale[i]=old_scale[i];
		note[i]=old_note[i];
	}

	ui_mode=PLAY;

	if (save){
		read_all_params_from_FLASH();
		store_globals_into_sram();
		write_all_params_to_FLASH();
	}
}

#define COEF_COEF (2.0 * 3.14159265358979323846 / 96000.0)

const float octaves[11]={1,2,4,8,16,32,64,128,256,512,1024};
const float TwelfthRootTwo[12]={1.05946309436, 1.12246204831087, 1.1892071150051, 1.25992104989823, 1.33483985417448, 1.41421356237875, 1.49830707688367,
		1.58740105197666, 1.68179283051751, 1.78179743629254, 1.88774862537721, 2.0};

#define TWELFTHROOTTWO 1.05946309436
#define ROOT 13.75
#define SLIDEREDITNOTE_LPF 0.980

void handle_edit_scale(void){
	uint8_t i;
	float freq;
	uint8_t octave_sel;
	uint8_t semitone_sel;
	uint8_t microtone_sel;
	uint8_t nanotone_sel;

	static uint32_t old_potadc[6]={0,0,0,0,0,0};

	static float slider_lpf[6]={0,0,0,0,0,0};

	if (diff(old_potadc[0],potadc_buffer[0+SLIDER_ADC_BASE]) > 100){
		old_potadc[0] = potadc_buffer[0+SLIDER_ADC_BASE];
		slider_lpf[0] = old_potadc[0];
	}
	if (diff(old_potadc[1],potadc_buffer[1+SLIDER_ADC_BASE]) > 100){
		old_potadc[1] = potadc_buffer[1+SLIDER_ADC_BASE];
		slider_lpf[1] = old_potadc[1];
	}
	if (diff(old_potadc[2],potadc_buffer[2+SLIDER_ADC_BASE]) > 50){
		old_potadc[2] = potadc_buffer[2+SLIDER_ADC_BASE];
		slider_lpf[2] = old_potadc[2];
	}
	if (diff(old_potadc[3],potadc_buffer[3+SLIDER_ADC_BASE]) > 50){
		old_potadc[3] = potadc_buffer[3+SLIDER_ADC_BASE];
		slider_lpf[3] = old_potadc[3];
	}


	//coef = 2 * pi * FREQ / SAMPLERATE
	if (!editscale_notelocked){
		octave_sel = (uint16_t)(slider_lpf[0]/374); //0..10

		semitone_sel = (uint16_t)(slider_lpf[1]/342); //0..6

		microtone_sel = (uint16_t)(slider_lpf[2]/79); //0..51
		nanotone_sel = (uint16_t)(slider_lpf[3]/300); //0..13

		freq = ROOT * octaves[octave_sel] * TwelfthRootTwo[semitone_sel] * exp_1voct[microtone_sel] * exp_1voct[nanotone_sel];

		user_scalebank[scale[0]*21 +  note[0]] = COEF_COEF * freq;


	}

}

void set_default_user_scalebank(void){
	uint8_t i,j;

	for (j=0;j<NUMSCALES;j++){
		for (i=0;i<NUM_FILTS+1;i++)
			user_scalebank[i+j*21] = DEFAULT_user_scalebank[i];
	}

	/*
	uint32_t sz;
	uint8_t *src;
	uint8_t *dst;

	sz=231*4;
	src = (uint8_t *)DEFAULT_user_scalebank;
	dst = (uint8_t *)user_scalebank;

	while (sz--) *dst++ = *src++;
	*/
}


void check_rotary_pressed_repeatedly(void){
	static uint16_t state=0;
	static uint16_t ctr=0;

	ctr++;
	if (ctr==10000){ //User took too long, start over
		state=0;
		ctr=0;
	}

	if (state==20){

		enter_edit_scale();

		state=0;
		ctr=0;
	}

	if ((state&1) && !ROTARY_SW){
		state++;
		ctr=0;
	}
	if (!(state&1) && ROTARY_SW){
		state++;
		ctr=0;
	}
}

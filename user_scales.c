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
extern float trackcomp[NUM_CHANNELS];
extern int16_t trackoffset[NUM_CHANNELS];
extern enum Env_Out_Modes env_track_mode;

extern float voltoct_pwm_tracking;

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
uint8_t editscale_tracklocked=0;
uint8_t editscale_voctlocked=0;

extern uint8_t note[NUM_CHANNELS];
extern uint8_t scale[NUM_CHANNELS];
extern uint8_t scale_bank[NUM_CHANNELS];

extern enum UI_Modes ui_mode;


extern __IO uint16_t potadc_buffer[NUM_ADC3S];


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
	editscale_tracklocked=1;
	editscale_voctlocked=1;

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
const float TwelfthRootTwo[12]={1.0, 1.05946309436, 1.12246204831087, 1.1892071150051, 1.25992104989823, 1.33483985417448, 1.41421356237875, 1.49830707688367, 1.58740105197666, 1.68179283051751, 1.78179743629254, 1.88774862537721};

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

	if (diff(old_potadc[0],potadc_buffer[0+SLIDER_ADC_BASE]) > 25){
		old_potadc[0] = potadc_buffer[0+SLIDER_ADC_BASE];
		slider_lpf[0] = old_potadc[0];
	}
	if (diff(old_potadc[1],potadc_buffer[1+SLIDER_ADC_BASE]) > 25){
		old_potadc[1] = potadc_buffer[1+SLIDER_ADC_BASE];
		slider_lpf[1] = old_potadc[1];
	}
	if (diff(old_potadc[2],potadc_buffer[2+SLIDER_ADC_BASE]) > 25){
		old_potadc[2] = potadc_buffer[2+SLIDER_ADC_BASE];
		slider_lpf[2] = old_potadc[2];
	}
	if (diff(old_potadc[3],potadc_buffer[3+SLIDER_ADC_BASE]) > 25){
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

		if (env_track_mode!=ENV_SLOW) 
			user_scalebank[scale[0]*21 +  note[0]] = COEF_COEF * freq;

		if (env_track_mode!=ENV_FAST && env_track_mode!=ENV_VOLTOCT)
			user_scalebank[scale[5]*21 +  note[5]] = COEF_COEF * freq;
		

	}

}

#define CENTER_PLATEAU 80
void handle_edit_voct(void)
{
	uint32_t track_adc;
	float t_f;
	static float voct_lpf;

	if (!editscale_voctlocked){

		t_f = potadc_buffer[4+SLIDER_ADC_BASE] * 0.9; //coarse
		t_f += potadc_buffer[5+SLIDER_ADC_BASE] * 0.1; //fine
		voct_lpf *= 0.99;
		voct_lpf += t_f*0.01;

		track_adc = (uint32_t)voct_lpf;

		if (track_adc > (2047 + CENTER_PLATEAU))
			voltoct_pwm_tracking 	= exp_1voct[(track_adc-(2047+CENTER_PLATEAU)) >> 6]; //2048..4095 => exp_1voct[0..15] or 1.0 to ~1.05

		else if (track_adc < (2047 - CENTER_PLATEAU))
			voltoct_pwm_tracking	= 1.0 / exp_1voct[((2047-CENTER_PLATEAU) - track_adc) >> 6]; //0..2047 => 1/exp_1voct[0..15] or 1.0 to ~0.95

		else
			voltoct_pwm_tracking 	= 1.0;

		if (voltoct_pwm_tracking < 0.5 || voltoct_pwm_tracking > 2.0) voltoct_pwm_tracking = 1.0; //sanity check!


	}

}


void handle_edit_tracking(void){

	uint16_t track_adc;

	if (!editscale_tracklocked){

		track_adc=potadc_buffer[4+SLIDER_ADC_BASE];

		if (track_adc>2047){
			if (env_track_mode!=ENV_SLOW) trackcomp[0] = exp_1voct[(track_adc-2048) >> 6]; //2048..4095 => exp_1voct[0..15] or 1.0 to ~1.05
			if (env_track_mode!=ENV_FAST  && env_track_mode!=ENV_VOLTOCT) trackcomp[1] = exp_1voct[(track_adc-2048) >> 6]; //2048..4095 => exp_1voct[0..15] or 1.0 to ~1.05
		}else{
			if (env_track_mode!=ENV_SLOW) trackcomp[0] = 1.0 / exp_1voct[(2047-track_adc) >> 6]; //0..2047 => 1/exp_1voct[0..15] or 1.0 to ~0.95
			if (env_track_mode!=ENV_FAST  && env_track_mode!=ENV_VOLTOCT) trackcomp[1] = 1.0 / exp_1voct[(2047-track_adc) >> 6]; //0..2047 => 1/exp_1voct[0..15] or 1.0 to ~0.95
		}

		if (trackcomp[0]<0.5 || trackcomp[0]>2.0) trackcomp[0]=1.0; //sanity check!
		if (trackcomp[1]<0.5 || trackcomp[1]>2.0) trackcomp[1]=1.0; //sanity check!

		track_adc=potadc_buffer[5+SLIDER_ADC_BASE];

		if (track_adc>2047){
			if (env_track_mode!=ENV_SLOW) trackoffset[0] = (track_adc-2048) >> 7; //0..31
			if (env_track_mode!=ENV_FAST  && env_track_mode!=ENV_VOLTOCT) trackoffset[1] = (track_adc-2048) >> 7; //0..31
		}else{
			if (env_track_mode!=ENV_SLOW) trackoffset[0] = 0-((2047-track_adc) >> 7); //-31..0
			if (env_track_mode!=ENV_FAST  && env_track_mode!=ENV_VOLTOCT) trackoffset[1] = 0-((2047-track_adc) >> 7); //-31..0
		}
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

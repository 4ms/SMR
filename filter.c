/*
 * filter.c - DSP bandpass resonant filter
 *
 * Author: Dan Green (danngreen1@gmail.com), Hugo Paris (hugoplho@gmail.com)
 * Algorithm based on work by Max Matthews and Julius O. Smith III, "Methods for Synthesizing Very High Q Parametrically Well Behaved Two Pole Filters", as published here: https://ccrma.stanford.edu/~jos/smac03maxjos/smac03maxjos.pdf
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

#include "arm_math.h"
#include "globals.h"
#include "filter.h"
#include "filter_coef.h"
#include "audio_util.h"
#include "dig_inouts.h"
#include "mem.h"
#include "log_4096.h"
#include "equal_pow_pan_padded.h"
#include "system_mode.h"
#include "params.h"
#include <stdio.h>
#include "rotation.h"
#include "twopass_calibration.h"

extern float user_scalebank[231];

/*
 * Stereo buffers : codec's buffer is 128 bytes (codec_BUFF_LEN)
 * Each half-transfer provides us with 64 bytes (DMA_xfer_BUFF_LEN)
 * which is 32 stereo samples (24 bits each) (STEREO_BUFSZ)
 * 16 Left and 16 Right channel samples (MONO_BUFSZ)
 *
 */
#define DMA_xfer_BUFF_LEN (codec_BUFF_LEN/2)
#define STEREO_BUFSZ (DMA_xfer_BUFF_LEN/2)
#define MONO_BUFSZ (STEREO_BUFSZ/2)
					
int32_t	left_buffer[MONO_BUFSZ], right_buffer[MONO_BUFSZ], filtered_buffer[MONO_BUFSZ], filtered_bufferR[MONO_BUFSZ];

extern float log_4096[4096];
extern float exp_4096[4096];

extern uint8_t note[NUM_CHANNELS];
extern uint8_t scale[NUM_CHANNELS];
extern uint8_t scale_bank[NUM_CHANNELS];

extern float ENVOUT_preload[NUM_CHANNELS];

extern const uint32_t slider_led[6];

// 2-pass Crossfade
extern enum Filter_Types filter_type;
extern enum Filter_Modes filter_mode;

extern float freq_nudge[NUM_CHANNELS];
extern float freq_shift[NUM_CHANNELS];

extern int8_t motion_fadeto_note[NUM_CHANNELS];
extern int8_t motion_fadeto_scale[NUM_CHANNELS];

extern float motion_morphpos[NUM_CHANNELS]; 

extern uint8_t slider_led_mode;

extern float channel_level[NUM_CHANNELS];

extern enum UI_Modes ui_mode;
extern uint8_t TESTING_MODE;

extern enum Env_Out_Modes env_track_mode;

//CHANNEL LEVELS/SLEW
extern float CHANNEL_LEVEL_LPF;
extern uint16_t potadc_buffer[NUM_ADC3S];
extern uint16_t adc_buffer[NUM_ADCS];

// **** two pass calibration variables ******
	// static float qval_b[NUM_CHANNELS]   = {1000,1000,1000,1000,1000,1000};	
	// float max_filter_out[NUM_FILTS];
	// float max_filter_out_buf;
	// float mean_filter_out[NUM_FILTS];
	// float mean_filter_out_buf;
	// int kk;
// ******************************************	

float *c_hiq[6];
float *c_loq[6];
float buf_a[NUM_CHANNELS][NUMSCALES][NUM_FILTS][3]; // buffer for first filter of two-pass
float buf[NUM_CHANNELS][NUMSCALES][NUM_FILTS][3]; 
float comp_gain[6];

float filter_out[NUM_FILTS][MONO_BUFSZ];

enum Filter_Types new_filter_type;
enum Filter_Modes new_filter_mode;
uint8_t filter_type_changed=0;
uint8_t filter_mode_changed=0;

extern uint32_t env_prepost_mode;

void change_filter_mode(enum Filter_Modes newmode){
	filter_mode_changed=1;
	new_filter_mode=newmode;
}

void change_filter_type(enum Filter_Types newtype){
	filter_type_changed=1;
	new_filter_type=newtype;
}

void process_audio_block(int16_t *src, int16_t *dst, uint16_t ht)
{
	//~78us static, ~104us morphing, every 166us = 6kHz ( = 96k / 16 samples)
	int16_t i,j;
	uint8_t k;
	uint8_t l;
	int32_t s;
	uint32_t t;
	float env_in;
	float f_start;
	float f_end;
	float f_blended;
	float *ff;
	float adc_lag[NUM_CHANNELS];
	float filter_out_a[NUM_FILTS][MONO_BUFSZ]; 			// first filter out for two-pass
	float filter_out_b[NUM_FILTS][MONO_BUFSZ]; 			// second filter out for two-pass
		
	static uint8_t old_scale[NUM_CHANNELS]={-1,-1,-1,-1,-1,-1};
	static uint8_t old_scale_bank[NUM_CHANNELS]={-1,-1,-1,-1,-1,-1};
	float tmp, fir, iir, iir_a;
	float c0,c0_a,c1,c2,c2_a;
	float a0,a1,a2;
	
	// Filter parameters
	static float qval_b[NUM_CHANNELS]   = {0,0,0,0,0,0};	
	static float qval_a[NUM_CHANNELS]   = {0,0,0,0,0,0};	
	static float qc[NUM_CHANNELS]   	= {0,0,0,0,0,0};
	extern uint32_t qval[NUM_CHANNELS];					
	float var_q, inv_var_q, var_f, inv_var_f;
	float pos_in_cf; 		 // % of Qknob position within crossfade region
	float ratio_a, ratio_b;  // two-pass filter crossfade ratios

// 	static float t_lpf[NUM_CHANNELS]={0.0,0.0,0.0,0.0,0.0,0.0};
	float level_lpf;
	static uint32_t poll_ctr[NUM_CHANNELS]= {0,0,0,0,0,0};
	static float prev_level[NUM_CHANNELS] = {0.0,0.0,0.0,0.0,0.0,0.0};
	static float level_goal[NUM_CHANNELS] = {0.0,0.0,0.0,0.0,0.0,0.0};

	uint8_t filter_num,channel_num;
	uint8_t scale_num;
	uint8_t nudge_filter_num;

	int32_t *ptmp_i32;

 	static float level_inc[NUM_CHANNELS] = {0,0,0,0,0,0};

 	static float t_morph = 0.0;

 	static int32_t sawL, sawR;

	if (filter_mode != TWOPASS){ // not mandatory but save CPU in most cases
		if (filter_type==BPRE && (
				scale_bank[0]>=NUMSCALEBANKS || scale_bank[1]>=NUMSCALEBANKS
				|| scale_bank[2]>=NUMSCALEBANKS || scale_bank[3]>=NUMSCALEBANKS
				|| scale_bank[4]>=NUMSCALEBANKS || scale_bank[5]>=NUMSCALEBANKS  ))
			change_filter_type(MAXQ);
	}

	// Convert 16-bit pairs to 24-bits stuffed into 32-bit integers: 1.6us
	audio_convert_2x16_to_stereo24(DMA_xfer_BUFF_LEN, src, left_buffer, right_buffer);

	if (filter_type_changed) filter_type=new_filter_type;


	//Determine the coef tables we're using for the active filters (Lo-Q and Hi-Q) for each channel
	//Also clear the buf[] history if we changed scales or banks, so we don't get artifacts
	//To-Do: move this somewhere else, so it runs on a timer
	for (i=0;i<NUM_CHANNELS;i++){

		//Range check scale_bank and scale
		if (scale_bank[i]<0) scale_bank[i]=0;
		if (scale_bank[i]>=NUMSCALEBANKS && scale_bank[i]!=0xFF) scale_bank[i]=NUMSCALEBANKS-1;
		if (scale[i]<0) scale[i]=0;
		if (scale[i]>=NUMSCALES) scale[i]=NUMSCALES-1;

		if (scale_bank[i]!=old_scale_bank[i] || filter_type_changed){

			old_scale_bank[i]=scale_bank[i];

			ff=(float *)buf[i];
			for (j=0;j<(NUMSCALES*NUM_FILTS);j++) {
				*(ff+j)=0;
				*(ff+j+1)=0;
				*(ff+j+2)=0;
			}

			if (filter_type == MAXQ){
				// Equal temperament
				if (scale_bank[i] 		== 0){
					c_hiq[i]=(float *)(filter_maxq_coefs_Major); 					// Major scale/chords
				} else if (scale_bank[i]== 1){
					c_hiq[i]=(float *)(filter_maxq_coefs_Minor); 					// Minor scale/chords
				} else if (scale_bank[i]== 2){
					c_hiq[i]=(float *)(filter_maxq_coefs_western_eq);				// Western intervals
				} else if (scale_bank[i]== 3){
					c_hiq[i]=(float *)(filter_maxq_coefs_western_twointerval_eq);	// Western triads
				} else if (scale_bank[i]== 4){
					c_hiq[i]=(float *)(filter_maxq_coefs_twelvetone);				// Chromatic scale - each of the 12 western semitones spread on multiple octaves
				} else if (scale_bank[i]== 5){
					c_hiq[i]=(float *)(filter_maxq_coefs_diatonic_eq);				// Diatonic scale Equal

				// Just intonation
				} else if (scale_bank[i]== 6){
					c_hiq[i]=(float *)(filter_maxq_coefs_western); 					// Western Intervals
				} else if (scale_bank[i]== 7){
					c_hiq[i]=(float *)(filter_maxq_coefs_western_twointerval); 		// Western triads (pairs of intervals)
				} else if (scale_bank[i]== 8){
					c_hiq[i]=(float *)(filter_maxq_coefs_diatonic_just);			// Diatonic scale Just


				// Non-Western Tunings
				} else if (scale_bank[i]== 9){
					c_hiq[i]=(float *)(filter_maxq_coefs_indian);					// Indian pentatonic
				} else if (scale_bank[i]== 10){
					c_hiq[i]=(float *)(filter_maxq_coefs_shrutis);					// Indian Shrutis
				} else if (scale_bank[i]== 11){
					c_hiq[i]=(float *)(filter_maxq_coefs_mesopotamian);				// Mesopotamian
				} else if (scale_bank[i]== 12){
					c_hiq[i]=(float *)(filter_maxq_coefs_gamelan);					// Gamelan Pelog


				// Modern tunings
				} else if (scale_bank[i]== 13){
					c_hiq[i]=(float *)(filter_maxq_coefs_alpha_spread2);			// W.C.'s Alpha scale - selected notes A
				} else if (scale_bank[i]== 14){
					c_hiq[i]=(float *)(filter_maxq_coefs_alpha_spread1);			// W.C.'s Alpha scale - selected notes B
				} else if (scale_bank[i]== 15){
					c_hiq[i]=(float *)(filter_maxq_coefs_gammaspread1);				// W.C.'s Gamma scale - selected notes
				} else if (scale_bank[i]== 16){
					c_hiq[i]=(float *)(filter_maxq_coefs_17ET);						// 17 notes/oct
				} else if (scale_bank[i]== 17){
					c_hiq[i]=(float *)(filter_maxq_coefs_bohlen_pierce);			// Bohlen Pierce
				} else if (scale_bank[i]== 18){
					c_hiq[i]=(float *)(filter_maxq_coefs_B296);						// Buchla 296 EQ

				// User	Scales
				} else if (scale_bank[i]==NUMSCALEBANKS-1){ 						//user scalebank is the last scalebank
					c_hiq[i]=(float *)(user_scalebank);
				}

			} else if (filter_mode != TWOPASS && filter_type==BPRE){

				// Equal temperament
				if (scale_bank[i] 		== 0){
					c_hiq[i]=(float *)(filter_bpre_coefs_Major_800Q); 				// Major scale/chords
					c_loq[i]=(float *)(filter_bpre_coefs_Major_2Q); 				// Major scale/chords
				} else if (scale_bank[i]== 1){
					c_hiq[i]=(float *)(filter_bpre_coefs_Minor_800Q); 				// Minor scale/chords
					c_loq[i]=(float *)(filter_bpre_coefs_Minor_2Q); 				// Minor scale/chords
				} else if (scale_bank[i]== 2){
					c_hiq[i]=(float *)(filter_bpre_coefs_western_eq_800Q); 			// Western intervals	
					c_loq[i]=(float *)(filter_bpre_coefs_western_eq_2Q); 				
				} else if (scale_bank[i]== 3){
					c_hiq[i]=(float *)(filter_bpre_coefs_western_twointerval_eq_800Q); 	// Western triads			
					c_loq[i]=(float *)(filter_bpre_coefs_western_twointerval_eq_2Q); 				
				} else if (scale_bank[i]== 4){
					c_hiq[i]=(float *)(filter_bpre_coefs_twelvetone_800Q);			// Chromatic scale - each of the 12 western semitones spread on multiple octaves
					c_loq[i]=(float *)(filter_bpre_coefs_twelvetone_2Q);			// Chromatic scale - each of the 12 western semitones spread on multiple octaves
				} else if (scale_bank[i]== 5){
					c_hiq[i]=(float *)(filter_bpre_coefs_diatonic_eq_800Q);			// Diatonic scale
					c_loq[i]=(float *)(filter_bpre_coefs_diatonic_eq_2Q);			// Diatonic scale

				// Just intonation
				} else if (scale_bank[i]== 6){
					c_hiq[i]=(float *)(filter_bpre_coefs_western_800Q); 			// Western Intervals
					c_loq[i]=(float *)(filter_bpre_coefs_western_2Q); 				// Western Intervals
				} else if (scale_bank[i]== 7){
					c_hiq[i]=(float *)(filter_bpre_coefs_western_twointerval_800Q); // Western triads (pairs of intervals)
					c_loq[i]=(float *)(filter_bpre_coefs_western_twointerval_2Q); 	// Western triads (pairs of intervals)
				} else if (scale_bank[i]== 8){
					c_hiq[i]=(float *)(filter_bpre_coefs_diatonic_just_800Q);		// Diatonic scale
					c_loq[i]=(float *)(filter_bpre_coefs_diatonic_just_2Q);			// Diatonic scale


				// Non-western Tunings
				} else if (scale_bank[i]== 9){
					c_hiq[i]=(float *)(filter_bpre_coefs_indian_800Q);				// Indian pentatonic
					c_loq[i]=(float *)(filter_bpre_coefs_indian_2Q);				// Indian pentatonic
				} else if (scale_bank[i]== 10){
					c_hiq[i]=(float *)(filter_bpre_coefs_shrutis_800Q);				// Indian Shrutis
					c_loq[i]=(float *)(filter_bpre_coefs_shrutis_2Q);				// Indian Shrutis
				} else if (scale_bank[i]== 11){
					c_hiq[i]=(float *)(filter_bpre_coefs_mesopotamian_800Q);		// Mesopotamian
					c_loq[i]=(float *)(filter_bpre_coefs_mesopotamian_2Q);			// Mesopotamian
				} else if (scale_bank[i]== 12){
					c_hiq[i]=(float *)(filter_bpre_coefs_gamelan_800Q);				// Gamelan Pelog
					c_loq[i]=(float *)(filter_bpre_coefs_gamelan_2Q);				// Gamelan Pelog


				// modern tunings
				} else if (scale_bank[i]== 13){
					c_hiq[i]=(float *)(filter_bpre_coefs_alpha_spread2_800Q);		// W.C.'s Alpha scale - selected notes A
					c_loq[i]=(float *)(filter_bpre_coefs_alpha_spread2_2Q);			// W.C.'s Alpha scale - selected notes A
				} else if (scale_bank[i]== 14){
					c_hiq[i]=(float *)(filter_bpre_coefs_alpha_spread1_800Q);		// W.C.'s Alpha scale - selected notes B
					c_loq[i]=(float *)(filter_bpre_coefs_alpha_spread1_2Q);			// W.C.'s Alpha scale - selected notes B
				} else if (scale_bank[i]== 15){
					c_hiq[i]=(float *)(filter_bpre_coefs_gammaspread1_800Q);		// W.C.'s Gamma scale - selected notes
					c_loq[i]=(float *)(filter_bpre_coefs_gammaspread1_2Q);			// W.C.'s Gamma scale - selected notes
				} else if (scale_bank[i]== 16){
					c_hiq[i]=(float *)(filter_bpre_coefs_17ET_800Q);				// 17 notes/oct
					c_loq[i]=(float *)(filter_bpre_coefs_17ET_2Q);					// 17 notes/oct
				} else if (scale_bank[i]== 17){
					c_hiq[i]=(float *)(filter_bpre_coefs_bohlen_pierce_800Q);		// Bohlen Pierce
					c_loq[i]=(float *)(filter_bpre_coefs_bohlen_pierce_2Q);			// Bohlen Pierce
				} else if (scale_bank[i]== 18){
					c_hiq[i]=(float *)(filter_bpre_coefs_B296_800Q);				// Buhcla 296 EQ
					c_loq[i]=(float *)(filter_bpre_coefs_B296_2Q);					// Buhcla 296 EQ
				}

			}
		} 	// new scale bank or filter type changed
	}		// channels




	//############################### 2-PASS  ###################################
	
	if (filter_mode == TWOPASS){


	 // UPDATE QVAL
		param_read_q();


	  // CALCULATE FILTER OUTPUTS		
	  	//filter_out[0-5] are the note[]/scale[]/scale_bank[] filters. 
		//filter_out[6-11] are the morph destination values
		//filter_out[channel1-6][buffer_sample]

		for (channel_num=0; channel_num<NUM_CHANNELS; channel_num++)
		{
			filter_num=note[channel_num];
			scale_num=scale[channel_num];

			qc[channel_num]   	=  qval[channel_num];

		// QVAL ADJUSTMENTS

			// first filter max Q at noon on Q knob
			qval_a[channel_num]	= qc[channel_num] * 2;
			if 		(qval_a[channel_num] > 4095	){qval_a[channel_num]=4095;	}

			// limit q knob range on second filter
			if 		(qc[channel_num] <  3900){qval_b[channel_num]=1000;}
			else if (qc[channel_num] >= 3900){qval_b[channel_num]=1000 + (qc[channel_num] - 3900)*15 ;} //1000 to 3925
			
			
			// ************* Two-pass calibration PT 1/2 *************
				// 			if (qval_b[channel_num]<3910){ 			
				// 				kk += 1;
				// 				// update qval_b and max_filt_out every 100 samples
				// 				if (kk> 1000){
				// 					qval_b[channel_num]+=1;
				//  					max_filter_out[channel_num] = max_filter_out_buf;
				//  					if (mean_filter_out_buf < mean_filter_out[channel_num]){
				// 						mean_filter_out_buf = mean_filter_out[channel_num] + 100;					
				//  					}
				//  					mean_filter_out[channel_num] = mean_filter_out_buf; 					
				// 					max_filter_out_buf=0;
				// 					mean_filter_out_buf=0;
				// 					kk=0;
				// 				}	
				// 			}
			// *******************************************************
			
	
			// Q/RESONANCE: c0 = 1 - 2/(decay * samplerate), where decay is around 0.01 to 4.0
			c0_a = 1.0 - exp_4096[(uint32_t)(qval_a[channel_num] /1.4)+200]/10.0; //exp[200...3125]
			c0   = 1.0 - exp_4096[(uint32_t)(qval_b[channel_num] /1.4)+200]/10.0; //exp[200...3125]

			// FREQ: c1 = 2 * pi * freq / samplerate
			c1 = *(c_hiq[channel_num] + (scale_num*21) + filter_num);
			c1 *= freq_nudge[channel_num] * freq_shift[channel_num];
			if (c1>1.30899581) c1=1.30899581; //hard limit at 20k

			// CROSSFADE between the two filters
			if 		(qc[channel_num] < CF_MIN) 	{ratio_a=1.0f;}
			else if (qc[channel_num] > CF_MAX) 	{ratio_a=0.0f;}
			else {
				pos_in_cf 	= (qc[channel_num]-CF_MIN) / CROSSFADE_WIDTH;
				ratio_a  	= 1.0f - pos_in_cf;
			}
 			ratio_b = (1.0f - ratio_a);
//  			ratio_b = 0.2;
   			ratio_b *= 43801543.68f / twopass_calibration[(uint32_t)(qval_b[channel_num]-900)]; // FIXME: 43801543.68f gain could be directly printed into calibration vector
//  			ratio_b *= 43801543.68 / twopass_calibration[500]; // FIXME: 43801543.68f gain could be directly printed into calibration vector
// 			ratio_b *= 0.5;
			
		    // AMPLITUDE: Boost high freqs and boost low resonance
			c2_a  = (0.003 * c1) - (0.1*c0_a) + 0.102;
			c2    = (0.003 * c1) - (0.1*c0)   + 0.102;
			c2 *= ratio_b;

			if (channel_num & 1) ptmp_i32=right_buffer;
			else ptmp_i32=left_buffer;

			j=channel_num;
			for (i=0;i<MONO_BUFSZ;i++)
			{
			  // FIRST PASS (_a)
				buf_a[channel_num][scale_num][filter_num][2] = (c0_a * buf_a[channel_num][scale_num][filter_num][1] + c1 * buf_a[channel_num][scale_num][filter_num][0]) - c2_a * (*ptmp_i32++);
				buf_a[channel_num][scale_num][filter_num][0] = buf_a[channel_num][scale_num][filter_num][0] - (c1 * buf_a[channel_num][scale_num][filter_num][2]);

				buf_a[channel_num][scale_num][filter_num][1] = buf_a[channel_num][scale_num][filter_num][2];
				filter_out_a[j][i] =  buf_a[channel_num][scale_num][filter_num][1];

			  // SECOND PASS (_b)
				buf[channel_num][scale_num][filter_num][2] = (c0 * buf[channel_num][scale_num][filter_num][1] + c1 * buf[channel_num][scale_num][filter_num][0]) - c2  * (filter_out_a[j][i]);
				buf[channel_num][scale_num][filter_num][0] = buf[channel_num][scale_num][filter_num][0] - (c1 * buf[channel_num][scale_num][filter_num][2]);

				buf[channel_num][scale_num][filter_num][1] = buf[channel_num][scale_num][filter_num][2];
				filter_out_b[j][i] = buf[channel_num][scale_num][filter_num][1];

  				filter_out[j][i] = (ratio_a * filter_out_a[j][i]) - filter_out_b[j][i]; // output of filter two needs to be inverted to avoid phase cancellation
			
			// *********************** Two-pass calibration pt 2/2 *********************
				// 				if (filter_out[j][i]> max_filter_out_buf){
				// 					max_filter_out_buf = filter_out[j][i];
				// 				}
				// 				mean_filter_out_buf *= (1-1/900);
				// 				mean_filter_out_buf += fabsf(filter_out[j][i]) * 1/900;
			// **************************************************************************
			
			}

			// VOCT output
			if (env_track_mode==ENV_VOLTOCT) ENVOUT_preload[channel_num]=c1;


			// Calculate the morph destination filter:
			// Calcuate c1 and c2, which must be updated since the freq changed, and then calculate an entire filter for each channel that's morphing
			// (Although it makes for poor readability to duplicate the inner loop section above, we save critical CPU time to do it this way)
			if (motion_morphpos[channel_num]>0.0)
			{

				filter_num=motion_fadeto_note[channel_num];
				scale_num=motion_fadeto_scale[channel_num];

				//FREQ: c1 = 2 * pi * freq / samplerate
				c1 = *(c_hiq[channel_num] + (scale_num*21) + filter_num);
				c1 *= freq_nudge[channel_num];
				c1 *= freq_shift[channel_num];
				if (c1>1.30899581) c1=1.30899581; //hard limit at 20k

				//AMPLITUDE: Boost high freqs and boost low resonance
				c2_a  = (0.003 * c1) - (0.1*c0_a) + 0.102;
				c2    = (0.003 * c1) - (0.1*c0) + 0.102;
				c2 *= ratio_b;

				//Point to the left or right input buffer
				if (channel_num & 1) ptmp_i32=right_buffer;
				else ptmp_i32=left_buffer;

				j=channel_num+6;
				for (i=0;i<MONO_BUFSZ;i++)
				{
				  // FIRST PASS (_a)
					buf_a[channel_num][scale_num][filter_num][2] = (c0_a * buf_a[channel_num][scale_num][filter_num][1] + c1 * buf_a[channel_num][scale_num][filter_num][0]) - c2_a * (*ptmp_i32++);
					buf_a[channel_num][scale_num][filter_num][0] = buf_a[channel_num][scale_num][filter_num][0] - (c1 * buf_a[channel_num][scale_num][filter_num][2]);

					buf_a[channel_num][scale_num][filter_num][1] = buf_a[channel_num][scale_num][filter_num][2];
					filter_out_a[j][i] =  buf_a[channel_num][scale_num][filter_num][1];

				  // SECOND PASS (_b)
					buf[channel_num][scale_num][filter_num][2] = (c0 * buf[channel_num][scale_num][filter_num][1] + c1 * buf[channel_num][scale_num][filter_num][0]) - c2  * (filter_out_a[j][i]);
					buf[channel_num][scale_num][filter_num][0] = buf[channel_num][scale_num][filter_num][0] - (c1 * buf[channel_num][scale_num][filter_num][2]);

					buf[channel_num][scale_num][filter_num][1] = buf[channel_num][scale_num][filter_num][2];
					filter_out_b[j][i] = buf[channel_num][scale_num][filter_num][1];

  					filter_out[j][i] = (ratio_a * filter_out_a[j][i]) - filter_out_b[j][i]; // output of filter two needs to be inverted to avoid phase cancellation
				}

				// VOCT output with glissando
				if (env_track_mode==ENV_VOLTOCT && env_prepost_mode==PRE)
						ENVOUT_preload[channel_num] = (ENVOUT_preload[channel_num] * (1.0f-motion_morphpos[channel_num])) + (c1 * motion_morphpos[channel_num]);

			}
		}

	}

	
	
	//###################### 1-PASS ###################################
	
	else if (filter_mode != TWOPASS)
	{
		
		// UPDATE QVAL
		param_read_q();

		//Calculate filter_out[]
		//filter_out[0-5] are the note[]/scale[]/scale_bank[] filters. filter_out[6-11] are the morph destination values
		if (filter_type==MAXQ)
		{
			for (j=0;j<NUM_CHANNELS*2;j++){

				if (j<6)
					channel_num=j;
				else
					channel_num=j-6;

				if (j<6 || motion_morphpos[channel_num]!=0){

					// Set filter_num and scale_num to the Morph sources
					if (j<6) {
						filter_num=note[channel_num];
						scale_num=scale[channel_num];

					// Set filter_num and scale_num to the Morph dests
					} else {
						filter_num=motion_fadeto_note[channel_num];
						scale_num=motion_fadeto_scale[channel_num];
					}

					nudge_filter_num = filter_num + 1;
					if (nudge_filter_num>NUM_FILTS) nudge_filter_num=NUM_FILTS;

				//Q/RESONANCE: c0 = 1 - 2/(decay * samplerate), where decay is around 0.01 to 4.0
					c0 = 1.0 - exp_4096[(uint32_t)(qval[channel_num]/1.4)+200]/10.0; //exp[200...3125]

				//FREQ: c1 = 2 * pi * freq / samplerate
					// FREQ: c1 = 2 * pi * freq / samplerate
					c1 = *(c_hiq[channel_num] + (scale_num*21) + filter_num);
					c1 *= freq_nudge[channel_num];
					c1 *= freq_shift[channel_num];
					if (c1>1.30899581) c1=1.30899581; //hard limit at 20k


				//AMPLITUDE: Boost high freqs and boost low resonance
					c2= (0.003 * c1) - (0.1*c0) + 0.102;
					c2 *= ((4096.0-qval[channel_num])/1024.0) + 1.04;


					for (i=0;i<MONO_BUFSZ/(96000/SAMPLERATE);i++){
						if (channel_num & 1) tmp=right_buffer[i];
						else tmp=left_buffer[i];

						buf[channel_num][scale_num][filter_num][2] = (c0 * buf[channel_num][scale_num][filter_num][1] + c1 * buf[channel_num][scale_num][filter_num][0]) - c2 * tmp;
						iir = buf[channel_num][scale_num][filter_num][0] - (c1 * buf[channel_num][scale_num][filter_num][2]);
						buf[channel_num][scale_num][filter_num][0] = iir;

						buf[channel_num][scale_num][filter_num][1] = buf[channel_num][scale_num][filter_num][2];
						filter_out[j][i] = buf[channel_num][scale_num][filter_num][1];
					}
				}
			}


		} else { //filter_type==BPRE

			for (j=0;j<NUM_CHANNELS*2;j++){

				if (j<6)
					channel_num=j;
				else
					channel_num=j-6;

				if (j<6 || motion_morphpos[channel_num]!=0.0){

					// Set filter_num and scale_num to the Morph sources
					if (j<6) {
						filter_num=note[channel_num];
						scale_num=scale[channel_num];

					// Set filter_num and scale_num to the Morph dests
					} else {
						filter_num=motion_fadeto_note[channel_num];
						scale_num=motion_fadeto_scale[channel_num];
					}

					//Q vector
					var_f=freq_nudge[channel_num];
					if (var_f<0.002) var_f=0.0;
					if (var_f>0.998) var_f=1.0;
					inv_var_f=1.0-var_f;

					//Freq nudge vector
					nudge_filter_num = filter_num + 1;
					if (nudge_filter_num>NUM_FILTS) nudge_filter_num=NUM_FILTS;


					a0=*(c_loq[channel_num] + (scale_num*63) + (nudge_filter_num*3) + 0)*var_f + *(c_loq[channel_num] + (scale_num*63) + (filter_num*3) + 0)*inv_var_f;
					a1=*(c_loq[channel_num] + (scale_num*63) + (nudge_filter_num*3) + 1)*var_f + *(c_loq[channel_num] + (scale_num*63) + (filter_num*3) + 1)*inv_var_f;
					a2=*(c_loq[channel_num] + (scale_num*63) + (nudge_filter_num*3) + 2)*var_f + *(c_loq[channel_num] + (scale_num*63) + (filter_num*3) + 2)*inv_var_f;

					c0=*(c_hiq[channel_num] + (scale_num*63) + (nudge_filter_num*3) + 0)*var_f + *(c_hiq[channel_num] + (scale_num*63) + (filter_num*3) + 0)*inv_var_f;
					c1=*(c_hiq[channel_num] + (scale_num*63) + (nudge_filter_num*3) + 1)*var_f + *(c_hiq[channel_num] + (scale_num*63) + (filter_num*3) + 1)*inv_var_f;
					c2=*(c_hiq[channel_num] + (scale_num*63) + (nudge_filter_num*3) + 2)*var_f + *(c_hiq[channel_num] + (scale_num*63) + (filter_num*3) + 2)*inv_var_f;

					//Q vector
					if (qval[channel_num]>4065) {
						var_q=1.0;
						inv_var_q=0.0;
					} else {
						var_q=log_4096[qval[channel_num]];
						inv_var_q = 1.0-var_q;
					}

					c0=c0*var_q + a0*inv_var_q;
					c1=c1*var_q + a1*inv_var_q;
					c2=c2*var_q + a2*inv_var_q;


					for (i=0;i<MONO_BUFSZ/(96000/SAMPLERATE);i++){

						
						tmp= buf[channel_num][scale_num][filter_num][0];
						buf[channel_num][scale_num][filter_num][0] = buf[channel_num][scale_num][filter_num][1];

						//Odd input (left) goes to odd filters (1/3/5)
						//Even input (right) goes to even filters (2/4/6)
						if (filter_num & 1) iir=left_buffer[i] * c0;
						else iir=right_buffer[i] * c0;

						iir -= c1*tmp;
						fir= -tmp;
						iir -= c2*buf[channel_num][scale_num][filter_num][0];
						fir += iir;
						buf[channel_num][scale_num][filter_num][1]= iir;

						filter_out[j][i]= fir;

					}
				}
			}
		}
	} 			// Filter-mode
	

	// ##########################################################


	// MORPHING
	for (i=0;i<MONO_BUFSZ;i++){

		filtered_buffer[i]=0;
		filtered_bufferR[i]=0;
		
		update_morph();

		for (j=0;j<NUM_CHANNELS;j++){
		
			if (motion_morphpos[j]==0.0)
				f_blended = filter_out[j][i];
			else
				f_blended = (filter_out[j][i] * (1.0f-motion_morphpos[j])) + (filter_out[j+NUM_CHANNELS][i] * motion_morphpos[j]); // filter blending

			if (TESTING_MODE){
				channel_level[j]=1.0;
			}
		 	if (ui_mode==EDIT_SCALES){
				if (env_track_mode!=ENV_SLOW ) channel_level[0]=1.0;
				else channel_level[0]=0.0;

				if (env_track_mode!=ENV_FAST && env_track_mode!=ENV_VOLTOCT) channel_level[5]=1.0;
				else channel_level[5]=0.0;

				channel_level[1]=0.0;
				channel_level[2]=0.0;
				channel_level[3]=0.0;
				channel_level[4]=0.0;

		 	} else {

				poll_ctr[j] +=1;
				if (poll_ctr[j]>50){ // UPDATE RATE

					poll_ctr[j]=0;

					t = potadc_buffer[j+SLIDER_ADC_BASE];
					if (t<20) t=0;
					else t-=20;

					// We may want to add this back:
					// apply LPF (equivalent to maximum cvlag) to slider adc readout, at all times
					//t_lpf[j] *= 0.9523012275f;
					//t_lpf[j] += 0.04769877248f *t;
					//Attenuate the CV signal by the slider value
					//	level_lpf=((float)(adc_buffer[j+LEVEL_ADC_BASE])/4096.0) *  (float)(t_lpf[j])/4096.0;


					level_lpf=((float)(adc_buffer[j+LEVEL_ADC_BASE])/4096.0) *  (float)(t)/4096.0;
					if (level_lpf<=0.007) level_lpf=0.0;

					prev_level[j] = level_goal[j];

					level_goal[j] *= CHANNEL_LEVEL_LPF;
					level_goal[j] += (1.0f-CHANNEL_LEVEL_LPF)*level_lpf;

					level_inc[j] = (level_goal[j]-prev_level[j])/50.0;
					channel_level[j] = prev_level[j];
				}
				else // SMOOTH OUT DATA BETWEEN ADC READS
					channel_level[j] += level_inc[j];
		 	}
			
			// APPLY LEVEL TO AUDIO OUT

			if (j & 1)
				filtered_buffer[i] += (f_blended * channel_level[j]);
			else
				filtered_bufferR[i] += (f_blended * channel_level[j]);

		}

	}

	for (j=0;j<NUM_CHANNELS;j++)
	{
		f_blended = (filter_out[j][0] * (1.0f-motion_morphpos[j])) + (filter_out[j+NUM_CHANNELS][0] * motion_morphpos[j]);

		if (slider_led_mode==SHOW_CLIPPING && ui_mode==PLAY){
			if ((f_blended * channel_level[j]) > SLIDER_CLIP_LEVEL)
				LED_SLIDER_ON(slider_led[j]);
		}

		if (env_track_mode != ENV_VOLTOCT){
			if (f_blended>0)
				ENVOUT_preload[j]=f_blended;
			else
				ENVOUT_preload[j]=-1.0*f_blended;
		}
	}
	
	if (TESTING_MODE){
		for (i=0;i<MONO_BUFSZ;i++)
		{
			sawL+=0x10000;
			if (sawL>(1200000000)) sawL=-1200000000;//-2147483647;
			filtered_buffer[i]=sawL;

			// sawR+=0x24800;
			// if (sawR>(1200000000)) sawR=-1200000000;//-2147483647;
			// filtered_bufferR[i]=sawR;
			filtered_bufferR[i]=(left_buffer[i] + right_buffer[i]) / 2;
		}
	}

	audio_convert_stereo24_to_2x16(DMA_xfer_BUFF_LEN, filtered_buffer, filtered_bufferR, dst); //1.5us

	filter_type_changed=0;
}

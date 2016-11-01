/*
 * filter.c - DSP bandpass resonant filter
 *
 * Author: Dan Green (danngreen1@gmail.com)
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
#include "limiter.h"
//#include "compressor.h"

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

// Filter parameters
extern uint32_t qval[NUM_CHANNELS];					

// 2-pass Crossfade
float pos_in_cf; 		 // % of Qknob position within crossfade region
float ratio_a, ratio_b;  // two-pass filter crossfade ratios

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

extern enum Env_Out_Modes env_track_mode;

float *c_hiq[6];
float *c_loq[6];
float buf_a[NUM_CHANNELS][NUMSCALES][NUM_FILTS][3]; // buffer for first filter of two-pass
float buf[NUM_CHANNELS][NUMSCALES][NUM_FILTS][3]; 
float comp_gain[6];

float filter_out[NUM_FILTS][MONO_BUFSZ];

 
// ADC readout smoothing
static float prev_level[12] = {0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0};	// previous channel level
static float smooth_level[NUM_CHANNELS][MONO_BUFSZ];							// ADC readout of channel level smoothed between samples
static float prev_qval[12]  = {0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0};		// previous qval
static float smooth_qval[NUM_CHANNELS][MONO_BUFSZ];					 		// ADC readout of qval smoothed between samples
//


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

inline void check_input_clipping(int32_t left_signal, int32_t right_signal){
		if (left_signal>INPUT_LED_CLIP_LEVEL)
			LED_CLIPL_ON;
		else
			LED_CLIPL_OFF;

		if (right_signal>INPUT_LED_CLIP_LEVEL)
			LED_CLIPR_ON;
		else
			LED_CLIPR_OFF;
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
	float cross_point; 									// crossfade point between one and two pass filter
	float crossfade_a, crossfade_b;
		
	float max_val, threshold, thresh_compiled, thresh_val;
	static uint8_t old_scale[NUM_CHANNELS]={-1,-1,-1,-1,-1,-1};
	static uint8_t old_scale_bank[NUM_CHANNELS]={-1,-1,-1,-1,-1,-1};
	float var_q, inv_var_q, var_f, inv_var_f;
	register float tmp, fir, iir, iir_a;
	float c0,c0_a,c1,c2,c2_a;
	float norm_exp; 
	float a0,a1,a2;

	uint8_t filter_num,channel_num;
	uint8_t scale_num;
	uint8_t nudge_filter_num;

	// Q adjustments 
	static float qval_b[NUM_CHANNELS]   = {0,0,0,0,0,0};	
	static float qval_a[NUM_CHANNELS]   = {0,0,0,0,0,0};	
	static float qsum[NUM_CHANNELS] 	= {0,0,0,0,0,0};
	static float qc[NUM_CHANNELS]   	= {0,0,0,0,0,0};
	static int firstrunq 		 		= 1;
 	static int q_update_count 			= 0;

	// ADC readout smoothing
	static float prev_level[NUM_CHANNELS] = {0.0,0.0,0.0,0.0,0.0,0.0};	// previous channel level
	static float smooth_level[NUM_CHANNELS][MONO_BUFSZ];				// ADC readout of channel level smoothed between samples
	static float prev_qval[NUM_CHANNELS]  = {0.0,0.0,0.0,0.0,0.0,0.0};	// previous qval
	static float smooth_qval[NUM_CHANNELS][MONO_BUFSZ];					// ADC readout of qval smoothed between samples
	// 	

	if (filter_mode != TWOPASS){ // not mandatory but save CPU in most cases
		if (filter_type==BPRE && (
				scale_bank[0]>=NUMSCALEBANKS || scale_bank[1]>=NUMSCALEBANKS
				|| scale_bank[2]>=NUMSCALEBANKS || scale_bank[3]>=NUMSCALEBANKS
				|| scale_bank[4]>=NUMSCALEBANKS || scale_bank[5]>=NUMSCALEBANKS  ))
			change_filter_type(MAXQ);
	}
	
	// Convert 16-bit pairs to 24-bits stuffed into 32-bit integers: 1.6us
	audio_convert_2x16_to_stereo24(DMA_xfer_BUFF_LEN, src, left_buffer, right_buffer);

	update_slider_LEDs(); //To-Do: move this somewhere else, so it runs on a timer

	//Handle motion
	update_motion(); //To-Do: move this somewhere else, so it runs on a timer
	
	if (filter_type_changed)
		filter_type=new_filter_type;

				
	

	//############################### 2-PASS  ###################################
	
	if (filter_mode == TWOPASS){
		
	  // FILTER COEFFICIENTS
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

				if (scale_bank[i]==0){
					c_hiq[i]=(float *)(filter_maxq_coefs_western);
				} else if (scale_bank[i]==1){
					c_hiq[i]=(float *)(filter_maxq_coefs_indian);
				} else if (scale_bank[i]==2){
					c_hiq[i]=(float *)(filter_maxq_coefs_alpha_spread2);
				} else if (scale_bank[i]==3){
					c_hiq[i]=(float *)(filter_maxq_coefs_alpha_spread1);
				} else if (scale_bank[i]==4){
					c_hiq[i]=(float *)(filter_maxq_coefs_gammaspread1);
				} else if (scale_bank[i]==5){
					c_hiq[i]=(float *)(filter_maxq_coefs_17ET);
				} else if (scale_bank[i]==6){
					c_hiq[i]=(float *)(filter_maxq_coefs_twelvetone);
				} else if (scale_bank[i]==7){
					c_hiq[i]=(float *)(filter_maxq_coefs_diatonic);
				} else if (scale_bank[i]==8){
					c_hiq[i]=(float *)(filter_maxq_coefs_diatonic2);
				} else if (scale_bank[i]==9){
					c_hiq[i]=(float *)(filter_maxq_coefs_western_twointerval);
				} else if (scale_bank[i]==10){
					c_hiq[i]=(float *)(filter_maxq_coefs_mesopotamian);
				} else if (scale_bank[i]==11){
					c_hiq[i]=(float *)(filter_maxq_coefs_shrutis);
				} else if (scale_bank[i]==12){
					c_hiq[i]=(float *)(filter_maxq_coefs_B296);
				} else if (scale_bank[i]==13){
					c_hiq[i]=(float *)(filter_maxq_coefs_gamelan);
				} else if (scale_bank[i]==14){
					c_hiq[i]=(float *)(filter_maxq_coefs_bohlen_pierce);
 				} else if (scale_bank[i]==15){
 					c_hiq[i]=(float *)(filter_maxq_coefs_Major);
				} else if (scale_bank[i]==16){
					c_hiq[i]=(float *)(filter_maxq_coefs_Minor);
				} else if (scale_bank[i]==NUMSCALEBANKS-1){ //user scalebank is the last scalebank
					c_hiq[i]=(float *)(user_scalebank);
				}
			}
		}

	  // CALCULATE FILTER OUTPUTS		
	  	//filter_out[0-5] are the note[]/scale[]/scale_bank[] filters. 
		//filter_out[6-11] are the morph destination values
		//filter_out[channel1-6][buffer_sample]
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

			  //Freq nudge vector
				var_f=freq_nudge[channel_num];
				if (var_f<0.002) var_f=0.0;
				if (var_f>0.998) var_f=1.0;
				inv_var_f=1.0-var_f;

				nudge_filter_num = filter_num + 1;
				if (nudge_filter_num>NUM_FILTS) nudge_filter_num=NUM_FILTS;


			  // UPDATE QVAL
 			  param_read_q();
			   //param_read_one_q(channel_num);
// 
// 			  // EXTRA LP ON QVAL
// 				q_update_count+= 1;
// 				if (q_update_count == QLPF_UPDATEPERIOD){
// 					q_update_count=0;
// 					if (firstrunq){
// 						qsum[channel_num] = qval[channel_num] * QNUM;
// 						firstrunq=0;
// 					}
// 					qsum[channel_num] 	-= qsum[channel_num] / QNUM;
// 					qsum[channel_num] 	+= qval[channel_num];
// 					qc[channel_num]   	=  qsum[channel_num] / QNUM; 
// 				}
				qc[channel_num]   	=  qval[channel_num]; 
			  
			  // QVAL ADJUSTMENTS
				// first filter max Q at noon on Q knob 
				qval_a[channel_num]	= qc[channel_num] * 2;
				if 		(qval_a[channel_num] > 4095	){qval_a[channel_num]=4095;	}
				else if (qval_a[channel_num] < 0	){qval_a[channel_num]=0;   	}

			  // limit q knob range on second filter
				if 		(qc[channel_num] < 3900	){qval_b[channel_num]=1000;}
				else if (qc[channel_num] >= 3900){qval_b[channel_num]=1000 + (qc[channel_num] - 3900)*15 ;}
				if 		(qval_b[channel_num] > 4095	){qval_b[channel_num]=4095;}
				else if (qval_b[channel_num] < 0	){qval_b[channel_num]=0;} 						
																
			  //Q/RESONANCE: c0 = 1 - 2/(decay * samplerate), where decay is around 0.01 to 4.0
				c0_a = 1.0 - exp_4096[(uint32_t)(qval_a[channel_num]  /1.4)+200]/10.0; //exp[200...3125]
				c0   = 1.0 - exp_4096[(uint32_t)(qval_b[channel_num]/1.4)+200]/10.0; //exp[200...3125]

			  //FREQ: c1 = 2 * pi * freq / samplerate
				c1 = *(c_hiq[channel_num] + (scale_num*21) + filter_num);
                c1 *= freq_nudge[channel_num];				
				c1 *= freq_shift[channel_num];
				if (c1>1.30899581) c1=1.30899581; //hard limit at 20k

				if (env_track_mode==ENV_VOLTOCT){
					if (j<6)
						ENVOUT_preload[j]=c1;
					else if (j>=6 && env_prepost_mode==PRE)
						ENVOUT_preload[j-6] = (ENVOUT_preload[j-6] * (1.0f-motion_morphpos[j-6])) + (c1 * motion_morphpos[j-6]);
				}

			
			  // CROSSFADE
				if 		(qc[channel_num] < CF_MIN) 	{ratio_b = 0.0f;}
				else if (qc[channel_num] > CF_MAX) 	{ratio_b = 1.0f;}							
				else {
					pos_in_cf = ((qc[channel_num]-CF_MIN) / CROSSFADE_WIDTH) * 4095.0f;
					//ratio_a  	= epp_lut[(uint32_t)pos_in_cf];
					ratio_a  	= 1.0f-pos_in_cf;
				}
				ratio_b = (1.0f - ratio_a);
				//ratio_b /= (0.0048 * qval_b[channel_num]);
				ratio_b *= 0.5/(2.4*qval_b[channel_num]/1000.0);
				//ratio_a = 1-(qval_b[channel_num]/4096.0f);
					//ratio_a

			  //AMPLITUDE: Boost high freqs and boost low resonance
				c2_a  = (0.003 * c1) - (0.1*c0_a) + 0.102;
//				c2_a *= ((4096.0-qval_a[channel_num])/1024.0) + 1.04;
				c2    = (0.003 * c1) - (0.1*c0) + 0.102;
//				c2   *= ((4096.0-qval_b[channel_num])/1024.0) + 1.04;
				c2 *= ratio_b;
				
				for (i=0;i<MONO_BUFSZ/(96000/SAMPLERATE);i++){
					check_input_clipping(left_buffer[i], right_buffer[i]);

					if (channel_num & 1) tmp=right_buffer[i];
					else tmp=left_buffer[i];
					
				  // FIRST PASS (_a)
					buf_a[channel_num][scale_num][filter_num][2] = (c0_a * buf_a[channel_num][scale_num][filter_num][1] + c1 * buf_a[channel_num][scale_num][filter_num][0]) - c2_a * tmp;
					//buf_a[channel_num][scale_num][filter_num][2] *= ratio_a;
					iir_a = buf_a[channel_num][scale_num][filter_num][0] - (c1 * buf_a[channel_num][scale_num][filter_num][2]);
					buf_a[channel_num][scale_num][filter_num][0] = iir_a;

					buf_a[channel_num][scale_num][filter_num][1] = buf_a[channel_num][scale_num][filter_num][2];
					filter_out_a[j][i] =  buf_a[channel_num][scale_num][filter_num][1];

				  // SECOND PASS (_b)
					buf[channel_num][scale_num][filter_num][2] = (c0 * buf[channel_num][scale_num][filter_num][1] + c1 * buf[channel_num][scale_num][filter_num][0]) - c2  * (filter_out_a[j][i]);
// 					buf[channel_num][scale_num][filter_num][2] *= ratio_b;
					iir = buf[channel_num][scale_num][filter_num][0] - (c1 * buf[channel_num][scale_num][filter_num][2]);
					buf[channel_num][scale_num][filter_num][0] = iir;

					buf[channel_num][scale_num][filter_num][1] = buf[channel_num][scale_num][filter_num][2];
					filter_out_b[j][i] = buf[channel_num][scale_num][filter_num][1]*1.25;
 							
//  					filter_out[j][i] = (ratio_a * filter_out_a[j][i] -  ratio_b * filter_out_b[j][i]/(0.0048 * qval_b[channel_num])); // output of filter two needs to be inverted to avoid phase cancellation
// 					filter_out[j][i] = (((float)(ratio_a) * filter_out_a[j][i]/1000.0f) -  ((float)(ratio_b) * filter_out_b[j][i] * 10 / ( 48 * qval_b[channel_num]))); // output of filter two needs to be inverted to avoid phase cancellation
 					filter_out[j][i] = ((ratio_a * filter_out_a[j][i]) - (filter_out_b[j][i])); // output of filter two needs to be inverted to avoid phase cancellation
//					filter_out[j][i] = (0.3 * (ratio_b + ratio_a) * filter_out_b[j][i]);//(0.0048 * qval_b[channel_num])); // output of filter two needs to be inverted to avoid phase cancellation
			
				}	// Buffer elements 			
			}		// not morphing
		}			// All channels	
	} 				// Filter type 			
	

	
	
	//###################### ORIGINAL 1-PASS ###################################
	
	else if (filter_mode != TWOPASS){
		
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

				if (filter_type==MAXQ){
					if (scale_bank[i]==0){
						c_hiq[i]=(float *)(filter_maxq_coefs_western);
					} else if (scale_bank[i]==1){
						c_hiq[i]=(float *)(filter_maxq_coefs_indian);
					} else if (scale_bank[i]==2){
						c_hiq[i]=(float *)(filter_maxq_coefs_alpha_spread2);
					} else if (scale_bank[i]==3){
						c_hiq[i]=(float *)(filter_maxq_coefs_alpha_spread1);
					} else if (scale_bank[i]==4){
						c_hiq[i]=(float *)(filter_maxq_coefs_gammaspread1);
					} else if (scale_bank[i]==5){
						c_hiq[i]=(float *)(filter_maxq_coefs_17ET);
					} else if (scale_bank[i]==6){
						c_hiq[i]=(float *)(filter_maxq_coefs_twelvetone);
					} else if (scale_bank[i]==7){
						c_hiq[i]=(float *)(filter_maxq_coefs_diatonic);
					} else if (scale_bank[i]==8){
						c_hiq[i]=(float *)(filter_maxq_coefs_diatonic2);
					} else if (scale_bank[i]==9){
						c_hiq[i]=(float *)(filter_maxq_coefs_western_twointerval);
					} else if (scale_bank[i]==10){
						c_hiq[i]=(float *)(filter_maxq_coefs_mesopotamian);
					} else if (scale_bank[i]==11){
						c_hiq[i]=(float *)(filter_maxq_coefs_shrutis);
					} else if (scale_bank[i]==12){
						c_hiq[i]=(float *)(filter_maxq_coefs_B296);
					} else if (scale_bank[i]==13){
						c_hiq[i]=(float *)(filter_maxq_coefs_gamelan);
					} else if (scale_bank[i]==14){
						c_hiq[i]=(float *)(filter_maxq_coefs_bohlen_pierce);

					} else if (scale_bank[i]==NUMSCALEBANKS-1){ //user scalebank is the last scalebank
						c_hiq[i]=(float *)(user_scalebank);
					}


				} else if (filter_type==BPRE){

					if (scale_bank[i]==0){
						c_hiq[i]=(float *)(filter_bpre_coefs_western_800Q);
						c_loq[i]=(float *)(filter_bpre_coefs_western_2Q);

					} else if (scale_bank[i]==1){
						c_hiq[i]=(float *)(filter_bpre_coefs_indian_800Q);
						c_loq[i]=(float *)(filter_bpre_coefs_indian_2Q);

					} else if (scale_bank[i]==2){
						c_hiq[i]=(float *)(filter_bpre_coefs_alpha_spread2_800Q);
						c_loq[i]=(float *)(filter_bpre_coefs_alpha_spread2_2Q);

					} else if (scale_bank[i]==3){
						c_hiq[i]=(float *)(filter_bpre_coefs_alpha_spread1_800Q);
						c_loq[i]=(float *)(filter_bpre_coefs_alpha_spread1_2Q);

					} else if (scale_bank[i]==4){
						c_hiq[i]=(float *)(filter_bpre_coefs_gammaspread1_800Q);
						c_loq[i]=(float *)(filter_bpre_coefs_gammaspread1_2Q);

					} else if (scale_bank[i]==5){
						c_hiq[i]=(float *)(filter_bpre_coefs_17ET_800Q);
						c_loq[i]=(float *)(filter_bpre_coefs_17ET_2Q);

					} else if (scale_bank[i]==6){
						c_hiq[i]=(float *)(filter_bpre_coefs_twelvetone_800Q);
						c_loq[i]=(float *)(filter_bpre_coefs_twelvetone_2Q);

					} else if (scale_bank[i]==7){
						c_hiq[i]=(float *)(filter_bpre_coefs_diatonic_800Q);
						c_loq[i]=(float *)(filter_bpre_coefs_diatonic_2Q);

					} else if (scale_bank[i]==8){
						c_hiq[i]=(float *)(filter_bpre_coefs_diatonic2_800Q);
						c_loq[i]=(float *)(filter_bpre_coefs_diatonic2_2Q);

					} else if (scale_bank[i]==9){
						c_hiq[i]=(float *)(filter_bpre_coefs_western_twointerval_800Q);
						c_loq[i]=(float *)(filter_bpre_coefs_western_twointerval_2Q);

					} else if (scale_bank[i]==10){
						c_hiq[i]=(float *)(filter_bpre_coefs_mesopotamian_800Q);
						c_loq[i]=(float *)(filter_bpre_coefs_mesopotamian_2Q);

					} else if (scale_bank[i]==11){
						c_hiq[i]=(float *)(filter_bpre_coefs_shrutis_800Q);
						c_loq[i]=(float *)(filter_bpre_coefs_shrutis_2Q);

					} else if (scale_bank[i]==12){
						c_hiq[i]=(float *)(filter_bpre_coefs_B296_800Q);
						c_loq[i]=(float *)(filter_bpre_coefs_B296_2Q);

					} else if (scale_bank[i]==13){
						c_hiq[i]=(float *)(filter_bpre_coefs_gamelan_800Q);
						c_loq[i]=(float *)(filter_bpre_coefs_gamelan_2Q);

					} else if (scale_bank[i]==14){
						c_hiq[i]=(float *)(filter_bpre_coefs_bohlen_pierce_800Q);
						c_loq[i]=(float *)(filter_bpre_coefs_bohlen_pierce_2Q);

					} else {
						scale_bank[i]=0;
						c_hiq[i]=(float *)(filter_bpre_coefs_western_800Q);
						c_loq[i]=(float *)(filter_bpre_coefs_western_2Q);

					}
				}
			} 	// new scale bank or filter type changed
		}		// channels



		//Calculate filter_out[]
		//filter_out[0-5] are the note[]/scale[]/scale_bank[] filters. filter_out[6-11] are the morph destination values
		if (filter_type==MAXQ){
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


					//Freq nudge vector
					var_f=freq_nudge[channel_num];
					if (var_f<0.002) var_f=0.0;
					if (var_f>0.998) var_f=1.0;
					inv_var_f=1.0-var_f;

					nudge_filter_num = filter_num + 1;
					if (nudge_filter_num>NUM_FILTS) nudge_filter_num=NUM_FILTS;

				//Q/RESONANCE: c0 = 1 - 2/(decay * samplerate), where decay is around 0.01 to 4.0
					c0 = 1.0 - exp_4096[(uint32_t)(qval[channel_num]/1.4)+200]/10.0; //exp[200...3125]

				//FREQ: c1 = 2 * pi * freq / samplerate
					c1 = *(c_hiq[channel_num] + (scale_num*21) + nudge_filter_num)*var_f + *(c_hiq[channel_num] + (scale_num*21) + filter_num)*inv_var_f;
					c1 *= freq_shift[channel_num];
					if (c1>1.30899581) c1=1.30899581; //hard limit at 20k

				//AMPLITUDE: Boost high freqs and boost low resonance
					c2= (0.003 * c1) - (0.1*c0) + 0.102;
					c2 *= ((4096.0-qval[channel_num])/1024.0) + 1.04;


					for (i=0;i<MONO_BUFSZ/(96000/SAMPLERATE);i++){
						check_input_clipping(left_buffer[i], right_buffer[i]);

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
		} else {

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

						//Input clipping
						check_input_clipping(left_buffer[i], right_buffer[i]);

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
		

		for (j=0;j<NUM_CHANNELS;j++){
		
			//When blending, 1.5us/sample. When static, 0.7us/sample = 11-24us total

			if (motion_morphpos[j]==0)
				f_blended = filter_out[j][i];
			else
				f_blended = (filter_out[j][i] * (1.0f-motion_morphpos[j])) + (filter_out[j+NUM_CHANNELS][i] * motion_morphpos[j]); // filter blending


			// update current value
// 			if (l=1){
			param_read_one_channel_level(j);
// 			l=0;
// 			}
// 			else{l++;}
			
			// APPLY LEVEL TO AUDIO OUT
			  	// apply level
				if (j & 1)
					s = filtered_buffer[i]  + (f_blended * channel_level[j]);
				else
					s = filtered_bufferR[i] + (f_blended * channel_level[j]);
				
				
				asm("ssat %[dst], #32, %[src]" : [dst] "=r" (s) : [src] "r" (s));

				if (j & 1)
					filtered_buffer[i] = (int32_t)(s);
				else
					filtered_bufferR[i] = (int32_t)(s);
				
				
				if (ui_mode==PLAY){
					if ((f_blended * (channel_level[j])) > SLIDER_CLIP_LEVEL){
// 					if ((f_blended * (channel_level[j] + smooth_level[j][i])) > SLIDER_CLIP_LEVEL){
						if (slider_led_mode==SHOW_CLIPPING){
							LED_SLIDER_ON(slider_led[j]);
						}

					}
				}

			if (env_track_mode != ENV_VOLTOCT){
				if (f_blended>0)
					ENVOUT_preload[j]=f_blended;
				else
					ENVOUT_preload[j]=-1.0*f_blended;
			}

		}

	}

	audio_convert_stereo24_to_2x16(DMA_xfer_BUFF_LEN, filtered_buffer, filtered_bufferR, dst); //1.5us

	filter_type_changed=0;

}

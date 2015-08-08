/*
 * filter.c

 *
 *  Created on: Nov 8, 2014
 *      Author: design
 */

#include "arm_math.h"
#include "globals.h"
#include "filter.h"
#include "filter_coef.h"
#include "audio_util.h"
#include "inouts.h"
#include "mem.h"
#include "log_4096.h"
//#include "exp_4096.h"
#include "system_mode.h"
#include "params.h"

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
extern float loga_4096[4096];
extern float exp_4096[4096];

extern uint8_t note[NUM_CHANNELS];
extern uint8_t scale[NUM_CHANNELS];
extern uint8_t scale_bank[NUM_CHANNELS];
extern uint16_t rotate_to_next_scale;

extern uint8_t g_error;

//extern uint32_t env_prepost_mode;

extern uint32_t ENVOUT_PWM[NUM_CHANNELS];
extern float ENVOUT_preload[NUM_CHANNELS];


extern const uint32_t slider_led[6];

extern __IO uint16_t adc_buffer[NUM_ADCS];
extern __IO uint16_t potadc_buffer[NUM_ADC3S];

extern uint32_t qval[NUM_CHANNELS];

//extern float spectral_readout[NUM_FILTS];

//extern uint8_t strike;
extern enum Filter_Types filter_type;

extern float freq_nudge[NUM_CHANNELS];
extern float freq_shift[NUM_CHANNELS];

extern int8_t motion_fadeto_note[NUM_CHANNELS];
extern int8_t motion_fadeto_scale[NUM_CHANNELS];
//extern int8_t motion_fadeto_bank[NUM_CHANNELS];

extern float motion_morphpos[NUM_CHANNELS];;

extern float envspeed_attack, envspeed_decay;

extern uint8_t slider_led_mode;

extern float channel_level[NUM_CHANNELS];

extern enum UI_Modes ui_mode;

float *c_hiq[6];
float *c_loq[6];
float buf[NUM_CHANNELS][NUMSCALES][NUM_FILTS][3];

//uint32_t env_trigout[NUM_CHANNELS];

enum Filter_Types new_filter_type;
uint8_t filter_type_changed=0;

void change_filter_type(enum Filter_Types newtype){

	filter_type_changed=1;
	new_filter_type=newtype;

}

inline void check_input_clipping(int32_t left_signal, int32_t right_signal){
//	if (!(left_signal & 0x80000000)){ //positive number
		if (left_signal>INPUT_LED_CLIP_LEVEL)
			LED_CLIPL_ON;
		else
			LED_CLIPL_OFF;
//	} else
//		LED_CLIPL_OFF;

//	if (!(right_signal & 0x80000000)){ //positive number
		if (right_signal>INPUT_LED_CLIP_LEVEL)
			LED_CLIPR_ON;
		else
			LED_CLIPR_OFF;
//	} else
//		LED_CLIPR_OFF;
}

void process_audio_block(int16_t *src, int16_t *dst, uint16_t ht)
{
	//DEBUGA_ON(DEBUG0); //100us, 125us when morphing. Runs @5.26kHz-->83k??
	int16_t i,j;
	uint8_t k;
	int32_t s;
	uint32_t t;

//	static float envelope[NUM_CHANNELS];
	float env_in;

	float f_start;
	float f_end;
	float f_blended;

	float *ff;

	float adc_lag[NUM_CHANNELS];

	float filter_out[NUM_FILTS][MONO_BUFSZ];

	static uint8_t old_scale[NUM_CHANNELS]={-1,-1,-1,-1,-1,-1};
	static uint8_t old_scale_bank[NUM_CHANNELS]={-1,-1,-1,-1,-1,-1};

	float var_q, inv_var_q, var_f, inv_var_f;
	register float tmp, fir, iir;
	float c0,c1,c2;
	float a0,a1,a2;
	float freq_comp[NUM_CHANNELS];

	uint8_t filter_num,channel_num;
	uint8_t scale_num;

	uint8_t nudge_filter_num;

	//~78us static, ~104us morphing, every 166us = 6kHz ( = 96k / 16 samples)

	if (filter_type==BPRE && (
			scale_bank[0]>=NUMSCALEBANKS || scale_bank[1]>=NUMSCALEBANKS
			|| scale_bank[2]>=NUMSCALEBANKS || scale_bank[3]>=NUMSCALEBANKS
			|| scale_bank[4]>=NUMSCALEBANKS || scale_bank[5]>=NUMSCALEBANKS  ))
		change_filter_type(MAXQ);

	// Convert 16-bit pairs to 24-bits stuffed into 32-bit integers: 1.6us
	audio_convert_2x16_to_stereo24(DMA_xfer_BUFF_LEN, src, left_buffer, right_buffer);

	update_slider_LEDs(); //can we move this?

	//Handle motion
	update_motion(); //also this?

	if (filter_type_changed)
		filter_type=new_filter_type;

	//Determine the coef tables we're using for the active filters (Lo-Q and Hi-Q) for each channel
	//Also clear the buf[] history if we changed scales or banks, so we don't get artifacts

	for (i=0;i<NUM_CHANNELS;i++){

		//range check scale_bank and scale
		if (scale_bank[i]<0) scale_bank[i]=0;
		if (scale_bank[i]>=NUMSCALEBANKS && scale_bank[i]!=0xFF) scale_bank[i]=NUMSCALEBANKS-1;
		if (scale[i]<0) scale[i]=0;
		if (scale[i]>=NUMSCALES) scale[i]=NUMSCALES-1;

		if (scale_bank[i]!=old_scale_bank[i] || filter_type_changed /*|| scale[i]!=old_scale[i]*/){

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

		}
	}



	//Calculate all the needed filters: about 14us for 6, 29us for 12
//	DEBUGA_ON(DEBUG2);

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
				freq_comp[channel_num] = 1.0;
				//freq_comp[channel_num] = 1.0/(float)exp_4096[(uint16_t)(c1*200)];

				//c2=8.0/c1;
				//if (c2>4095.0) c2=4095.0;
				//if (c2<0.0) c2=0.0;
				//freq_comp[channel_num] = 1.0/(float)log_4096[(uint16_t)(c2)];

			//AMPLITUDE: Boost high freqs and boost low resonance
			//	c2= 0.030 * c1 + 0.25 * (1-c0) + 0.005;
			//	c2= 0.030 * c1 + 1.0 * (1-c0) + 0.0015;
			//	c2= 0.030 * c1 + 0.005/loga_4096[qval[channel_num]] + 0.002;
			//	c2= (0.05 * c1) + (3.0/(qval[channel_num]+1)) + 0.0015;

				c2= (0.003 * c1) - (0.1*c0) + 0.102;
				c2 *= ((4096.0-qval[channel_num])/1024.0) + 1.04;


				for (i=0;i<MONO_BUFSZ/(96000/SAMPLERATE);i++){
					check_input_clipping(left_buffer[i], right_buffer[i]);

					//if (filter_num & 1) tmp=left_buffer[i];
					//else tmp=right_buffer[i];

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


//	DEBUGA_OFF(DEBUG2);


	for (i=0;i<MONO_BUFSZ;i++){

		filtered_buffer[i]=0;
		filtered_bufferR[i]=0;

		for (j=0;j<NUM_CHANNELS;j++){

			//when blending, 1.5us/sample. When static, 0.7us/sample = 11-24us total

			if (motion_morphpos[j]==0)
				f_blended = filter_out[j][i];
			else
				f_blended = (filter_out[j][i] * (1.0f-motion_morphpos[j])) + (filter_out[j+NUM_CHANNELS][i] * motion_morphpos[j]);

			//1.4us/sample = 22us total
			//if (channel_level[j]>0.0){

				if (j & 1)
					s = filtered_buffer[i] + (f_blended*channel_level[j]);
				else
					s = filtered_bufferR[i] + (f_blended*channel_level[j]);


				asm("ssat %[dst], #32, %[src]" : [dst] "=r" (s) : [src] "r" (s));

				if (j & 1)
					filtered_buffer[i] = (int32_t)(s);
				else
					filtered_bufferR[i] = (int32_t)(s);

				if (ui_mode==PLAY){
					if (((f_blended*channel_level[j]))>SLIDER_CLIP_LEVEL){
						if (slider_led_mode==SHOW_CLIPPING){
							LED_SLIDER_ON(slider_led[j]);
						}

					}
				}


			if (f_blended>0)
				ENVOUT_preload[j]=f_blended*(freq_comp[j]);
			else
				ENVOUT_preload[j]=-1.0*f_blended*(freq_comp[j]);


		}

	}

	audio_convert_stereo24_to_2x16(DMA_xfer_BUFF_LEN, filtered_buffer, filtered_bufferR, dst); //1.5us
	//DEBUGA_OFF(DEBUG0);
	filter_type_changed=0;

}

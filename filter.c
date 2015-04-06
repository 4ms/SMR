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

float adc_lag_attack=0;
float adc_lag_decay=0;

extern float log_4096[4096];
extern uint8_t rotate;
extern uint8_t spread;
extern uint8_t scale[NUM_CHANNELS];
extern uint8_t scale_bank[NUM_CHANNELS];

extern uint8_t g_error;

extern uint32_t env_prepost_mode;

extern uint32_t ENVOUT_PWM[NUM_CHANNELS];
extern const uint32_t clip_led[6];

extern uint8_t lock[NUM_CHANNELS];
extern uint8_t lock_pos[NUM_CHANNELS];

extern uint8_t filter_assign_table[NUM_CHANNELS];
extern uint8_t dest_filter_assign_table[NUM_CHANNELS];
extern float rot_dir[NUM_CHANNELS];
extern float spread_dir[NUM_CHANNELS];

extern uint8_t dest_scale[NUM_CHANNELS];

//extern uint8_t blend_mode;

extern uint8_t flag_update_LED_ring;

extern __IO uint16_t adc_buffer[NUM_ADCS+1];
extern uint32_t qval[NUM_CHANNELS];

//extern float spectral_readout[NUM_FILTS];

//extern uint8_t strike;

extern float freq_nudge[NUM_CHANNELS];

float assign_fade[NUM_CHANNELS]={0,0,0,0,0,0};

extern float envspeed_attack, envspeed_decay;

extern uint8_t SLIDER_LEDS;






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





/**
  * @brief  This function handles I2S RX buffer processing.
  * @param  src - pointer to source buffer
  * @param  dst - pointer to dest buffer
  * @param  sz -  samples per buffer
  * @retval none
  *
  * At Sample Rate of 48k, and buffer size of 128 samples (64 Left + 64 Right, thus 32 stereo samples per half-transfer)
  * we should be calling this function every 666us (32/48000)
  *
  */


float *c_hiq[6];
float *c_loq[6];
float buf[NUM_CHANNELS][NUMSCALES][NUM_FILTS][2];
float f_adc[NUM_CHANNELS];


void I2S_RX_CallBack(int16_t *src, int16_t *dst, uint16_t ht)
{
	DEBUGA_ON(DEBUG0);


	static float envelope[NUM_CHANNELS];
	float env_in;

	int32_t s;
	uint32_t t;
	float f_t;

	int16_t i,j;
	float f_start;
	float f_end;
	float f_blended;

	float *ff;

	float adc_lag[NUM_CHANNELS];

	float filter_out[NUM_FILTS][MONO_BUFSZ];

	uint8_t total_calc_filters=0;
	uint8_t nc_filter[12]={0,0,0,0,0,0,0,0,0,0,0,0};
	uint8_t nc_channel[12]={0,0,0,0,0,0,0,0,0,0,0,0};
	uint32_t need_calc=0;
	uint8_t k;

	static uint8_t old_scale[NUM_CHANNELS]={-1,-1,-1,-1,-1,-1};
	static uint8_t old_scale_bank[NUM_CHANNELS]={-1,-1,-1,-1,-1,-1};

	static float f_ctr=0;

	float var_q, inv_var_q, var_f, inv_var_f;
	register float tmp, fir, iir;
	float c0,c1,c2;
	float a0,a1,a2;
	uint8_t filter_num,channel_num;
	uint8_t scale_num;
	static uint8_t is_fading_down_across_scales[NUM_CHANNELS]={0,0,0,0,0,0};

	uint8_t nudge_filter_num;

	//~60us static, ~88us morphing



	//convert 16-bit pairs to 24-bits stuffed into 32-bit integers: 1.6us
	audio_convert_2x16_to_stereo24(DMA_xfer_BUFF_LEN, src, left_buffer, right_buffer);


	f_ctr+=0.01;
	if (f_ctr>1.0) f_ctr=0.0;

	// Calculate the CV/Slider level: 0.2us x 6 = 1.6us
	for (i=0;i<NUM_CHANNELS;i++){

		//1st order LPF:
		env_in=((float)adc_buffer[i+2])/4096.0f;

		if(f_adc[i] < env_in) {
			f_adc[i] *= adc_lag_attack;
			f_adc[i] += (1.0f-adc_lag_attack)*env_in;
		} else {
			f_adc[i] *= adc_lag_decay;
			f_adc[i] += (1.0f-adc_lag_decay)*env_in;
		}

		if (SLIDER_LEDS==SHOW_LEVEL){
			if (f_adc[i]>f_ctr)
				LED_ON((clip_led[i]));
			else
				LED_OFF((clip_led[i]));
		}
	}

	//Calculate assign_fade: 1.5us
	//Channel j should be the sum of filter_assign_table[j] (integer component) + assign_fade[j] (fractional component)
	//Thus if filter_assign_table[j]=2 and assign_fade[j]=0.25, the channel is essentially at f2.25 or 75% of f2 + 25% of f3.
	//Update assign_fade[] and inc/decrement filter_assign_table
	f_t=1.0/((adc_buffer[MORPH_ADC]+1.0)*1.5);



	for (j=0;j<NUM_CHANNELS;j++){

		if ((dest_filter_assign_table[j] != filter_assign_table[j]) || assign_fade[j]!=0 || dest_scale[j]!=scale[j])
		{

			//spread takes precedence over rot
			if (spread_dir[j]!=0.0){
				assign_fade[j]+=spread_dir[j]*f_t;
				rot_dir[j]=0;

			} else {
				assign_fade[j]+=rot_dir[j]*f_t;
			}

			if (assign_fade[j]>=1){

				filter_assign_table[j]=(filter_assign_table[j]+1) % NUM_FILTS;

				if (filter_assign_table[j]==0 && dest_scale[j]!=scale[j]){
					scale[j]=dest_scale[j];
					flag_update_LED_ring=1;

				}


				if (dest_filter_assign_table[j] == filter_assign_table[j] && dest_scale[j]==scale[j])
				{
					assign_fade[j]=0;
					is_fading_down_across_scales[j]=0;
					rot_dir[j]=0;
					spread_dir[j]=0;
				} else{
					assign_fade[j]-=1;
					flag_update_LED_ring=1;
				}
			}

			if (assign_fade[j]<=0){

				if (dest_filter_assign_table[j] == filter_assign_table[j]  && dest_scale[j]==scale[j]){ //We hit out final destination, so stop.
					assign_fade[j]=0;
					rot_dir[j]=0;
					spread_dir[j]=0;
					is_fading_down_across_scales[j]=0;

				} else {												//We are moving negative, or we hit a step along the way
					assign_fade[j]+=1;
					if (filter_assign_table[j]==0) filter_assign_table[j]=NUM_FILTS-1;
					else filter_assign_table[j]--;

					if (filter_assign_table[j]==(NUM_FILTS-1) && dest_scale[j]!=scale[j]){
						scale[j]=dest_scale[j];
						is_fading_down_across_scales[j]=1;
					}

					flag_update_LED_ring=1;
				}

			}

		}
	}


	//Figure out the coef tables we're drawing from (Lo-Q and Hi-Q) for each channel
	//Also clear the buf[] history if we changed scales or banks, so we don't get artifacts
	for (i=0;i<NUM_CHANNELS;i++){


		if (scale_bank[i]<0) scale_bank[i]=0;
		if (scale_bank[i]>=NUMSCALEBANKS) scale_bank[i]=NUMSCALEBANKS-1;
		if (scale[i]<0) scale[i]=0;
		if (scale[i]>=NUMSCALES) scale[i]=NUMSCALES-1;

		if (scale_bank[i]!=old_scale_bank[i] /*|| scale[i]!=old_scale[i]*/){

			old_scale_bank[i]=scale_bank[i];

			ff=(float *)buf[i];
			for (j=0;j<(NUMSCALES*NUM_FILTS);j++) *(ff+j)=0;


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
				//c_hiq[i]=(float *)(filter_bpre_coefs_gamma_800Q);
				//c_loq[i]=(float *)(filter_bpre_coefs_gamma_2Q);

			}
		}
	}



	//Calculate all the needed filters: about 14us for 6, 29us for 12
	//DEBUGA_ON(DEBUG2);

	//Calculate filter_out[]
	//filter_out[0-5] are the filter_assign_table filters. filter_out[6-11] are the morph destination/source values

	for (j=0;j<NUM_CHANNELS*2;j++){

		if (j<6)
			channel_num=j;
		else
			channel_num=j-6;

		if (j<6 || assign_fade[channel_num]!=0){

			if (j<6) {
				filter_num=filter_assign_table[channel_num];
				scale_num=scale[channel_num];

				//filter_num+=freqcv_bumpup[channel_num];
				//if (filter_num>=NUM_FILTS) filter_num=NUM_FILTS-1;


			}else{
				filter_num=(filter_assign_table[channel_num]+1) % NUM_FILTS;

				//filter_num+=freqcv_bumpup[channel_num];
				//if (filter_num>=NUM_FILTS) filter_num=NUM_FILTS-1;

				if (filter_num==0){
					if (is_fading_down_across_scales[channel_num])
						scale_num=(scale[channel_num] +1 ) % NUMSCALES;
					else
						scale_num=dest_scale[channel_num];

				} else {
					scale_num=scale[channel_num];
				}
			}

			//var_f=freq_nudge[filter_num];
			var_f=freq_nudge[channel_num];
			if (var_f<0.002) var_f=0.0;
			if (var_f>0.998) var_f=1.0;
			inv_var_f=1.0-var_f;

			//Freq fade
			nudge_filter_num = filter_num + 1;
			if (nudge_filter_num>NUM_FILTS) nudge_filter_num=NUM_FILTS;

			a0=*(c_loq[channel_num] + (scale_num*63) + (nudge_filter_num*3) + 0)*var_f + *(c_loq[channel_num] + (scale_num*63) + (filter_num*3) + 0)*inv_var_f;
			a1=*(c_loq[channel_num] + (scale_num*63) + (nudge_filter_num*3) + 1)*var_f + *(c_loq[channel_num] + (scale_num*63) + (filter_num*3) + 1)*inv_var_f;
			a2=*(c_loq[channel_num] + (scale_num*63) + (nudge_filter_num*3) + 2)*var_f + *(c_loq[channel_num] + (scale_num*63) + (filter_num*3) + 2)*inv_var_f;

			c0=*(c_hiq[channel_num] + (scale_num*63) + (nudge_filter_num*3) + 0)*var_f + *(c_hiq[channel_num] + (scale_num*63) + (filter_num*3) + 0)*inv_var_f;
			c1=*(c_hiq[channel_num] + (scale_num*63) + (nudge_filter_num*3) + 1)*var_f + *(c_hiq[channel_num] + (scale_num*63) + (filter_num*3) + 1)*inv_var_f;
			c2=*(c_hiq[channel_num] + (scale_num*63) + (nudge_filter_num*3) + 2)*var_f + *(c_hiq[channel_num] + (scale_num*63) + (filter_num*3) + 2)*inv_var_f;

/*
			a0=*(c_loq[channel_num] + (scale_num*63) + (filter_num*3) + 3)*var_f + *(c_loq[channel_num] + (scale_num*63) + filter_num*3 + 0)*inv_var_f;
			a1=*(c_loq[channel_num] + (scale_num*63) + (filter_num*3) + 4)*var_f + *(c_loq[channel_num] + (scale_num*63) + filter_num*3 + 1)*inv_var_f;
			a2=*(c_loq[channel_num] + (scale_num*63) + (filter_num*3) + 5)*var_f + *(c_loq[channel_num] + (scale_num*63) + filter_num*3 + 2)*inv_var_f;

			c0=*(c_hiq[channel_num] + scale_num*63 + filter_num*3 + 3)*var_f + *(c_hiq[channel_num] + (scale_num*63) + (filter_num*3) + 0)*inv_var_f;
			c1=*(c_hiq[channel_num] + scale_num*63 + filter_num*3 + 4)*var_f + *(c_hiq[channel_num] + (scale_num*63) + (filter_num*3) + 1)*inv_var_f;
			c2=*(c_hiq[channel_num] + scale_num*63 + filter_num*3 + 5)*var_f + *(c_hiq[channel_num] + (scale_num*63) + (filter_num*3) + 2)*inv_var_f;
*/
			//Q fade

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

				//Odd inputs go to odd filters numbers (1-20)
				if (!(left_buffer[i] & 0x80000000)){ //positive number
					if (left_buffer[i]>0x7F000000)
						LED_CLIPL_ON;
					else LED_CLIPL_OFF;
				}else LED_CLIPL_OFF;

				if (!(right_buffer[i] & 0x80000000)){ //positive number
					if (right_buffer[i]>0x7F000000)
						LED_CLIPR_ON;
					else LED_CLIPR_OFF;
				}else LED_CLIPR_OFF;

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

//	DEBUGA_OFF(DEBUG2);


	for (i=0;i<MONO_BUFSZ;i++){

		filtered_buffer[i]=0;
		filtered_bufferR[i]=0;

		for (j=0;j<NUM_CHANNELS;j++){

			//when blending, 1.5us/sample. When static, 0.7us/sample = 11-24us total
			if (assign_fade[j]==0){
				f_blended = filter_out[j][i];
			} else {
				//Rotate Morph fade
				f_blended = (filter_out[j][i] * (1.0f-assign_fade[j])) + (filter_out[j+NUM_CHANNELS][i] * assign_fade[j]);
			}



			//1.4us/sample = 22us total
			if (f_adc[j]>0.001){

				if (j & 1) s=filtered_buffer[i]+(f_blended*f_adc[j]);
				else s=filtered_bufferR[i]+(f_blended*f_adc[j]);

				asm("ssat %[dst], #32, %[src]" : [dst] "=r" (s) : [src] "r" (s));

				if (j & 1) filtered_buffer[i]=(int32_t)(s);
				else filtered_bufferR[i]=(int32_t)(s);

				if (SLIDER_LEDS==SHOW_CLIPPING){
					if (((f_blended*f_adc[j]))>0x60000000)
						LED_ON((clip_led[j]));
					else
						LED_OFF((clip_led[j]));
				}
			} else {
				if (SLIDER_LEDS==SHOW_CLIPPING) LED_OFF((clip_led[j]));
			}


			//CALCULATE ENVELOPE OUTPUTS: 0.96 - 1.5us/sample = 16-24us total
			//1st order LPF:
			if (f_blended>0) env_in=f_blended;
			else env_in=-1.0*f_blended;

			if(envelope[j] < env_in) {
				envelope[j] *= envspeed_attack;
				envelope[j] += (1.0-envspeed_attack)*env_in;
			} else {
				envelope[j] *= envspeed_decay;
				envelope[j] += (1.0-envspeed_decay)*env_in;
			}
			//Pre-CV (input) or Post-CV (output)
			if (env_prepost_mode==PRE) ENVOUT_PWM[j]=((uint32_t)envelope[j])>>16;
			else ENVOUT_PWM[j]=(uint32_t)(envelope[j]*f_adc[j])>>16;


		}

	}

	audio_convert_stereo24_to_2x16(DMA_xfer_BUFF_LEN, filtered_buffer, filtered_bufferR, dst); //1.5us
	DEBUGA_OFF(DEBUG0);

}
/*
inline float filter_PkBq(register float val, uint8_t filter_num, float var1, uint8_t qval){
	DEBUG_ON(DEBUG2);

   static float buf[NUM_FILTS][2];
   float c0,c1,c2,c3,c4,c5;
   register float tmp, fir, iir;
   tmp= buf[filter_num][0];

   buf[filter_num][0]=buf[filter_num][1];

   if (qval==50){
	   c0=filter_pkbq_coefs_50Q[filter_num][0];
	   c1=filter_pkbq_coefs_50Q[filter_num][1];
	   c2=filter_pkbq_coefs_50Q[filter_num][2];
	   c3=filter_pkbq_coefs_50Q[filter_num][3];
	   c4=filter_pkbq_coefs_50Q[filter_num][4];
	   c5=filter_pkbq_coefs_50Q[filter_num][5];
   } else if (qval==200){
	   c0=filter_pkbq_coefs_200Q[filter_num][0];
	   c1=filter_pkbq_coefs_200Q[filter_num][1];
	   c2=filter_pkbq_coefs_200Q[filter_num][2];
	   c3=filter_pkbq_coefs_200Q[filter_num][3];
	   c4=filter_pkbq_coefs_200Q[filter_num][4];
	   c5=filter_pkbq_coefs_200Q[filter_num][5];
   }
	iir= val * c0;
	iir -= c1*tmp;
	fir= c2*tmp;
	iir -= c3*buf[filter_num][0];
	fir += c4*buf[filter_num][0];
	fir += c5*iir;

   buf[filter_num][1]= iir; val= fir;

   DEBUG_OFF(DEBUG2);

   return val;
}
*/

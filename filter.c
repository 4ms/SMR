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

extern float log_4096[4096];
extern uint8_t rotate;
extern uint8_t spread;
extern uint8_t scale;
extern uint8_t scale_bank;

extern float adc_lag_attack;
extern float adc_lag_decay;

extern uint8_t g_error;

extern uint32_t envout_mode;

extern uint32_t ENVOUT_PWM[NUM_ACTIVE_FILTS];
extern const uint32_t clip_led[6];

extern uint8_t lock[NUM_ACTIVE_FILTS];
extern uint8_t lock_pos[NUM_ACTIVE_FILTS];

extern uint8_t filter_assign_table[NUM_ACTIVE_FILTS];
extern uint8_t dest_filter_assign_table[NUM_ACTIVE_FILTS];
extern float rot_dir[NUM_ACTIVE_FILTS];

//extern uint8_t blend_mode;

extern uint8_t flag_update_LED_ring;

extern __IO uint16_t adc_buffer[NUM_ADCS];

//extern float spectral_readout[NUM_FILTS];

//extern uint8_t strike;


float *c;
float *c_2q;
float buf[NUM_FILTS][2];




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




float assign_fade[NUM_ACTIVE_FILTS]={0,0,0,0,0,0};

void I2S_RX_CallBack(int16_t *src, int16_t *dst, uint16_t ht)
{
	DEBUG_ON(DEBUG0);

	static float envelope[NUM_ACTIVE_FILTS];
	float env_in;
	//float ga = exp(-1.0f/(SampleRate*AttackTimeInSecond));
	//float gr = exp(-1.0f/(SampleRate*ReleaseTimeInSeconds));
	float gAttack;
	float gDecay;

	if (ENVSPEED) {
		gAttack=0.99916883665;
		gDecay=0.99916883665;
	} else {
		gAttack=0.9998958;
		gDecay=0.9998958;
	}

	int32_t s;
	uint32_t t;
	float f_t;

	int16_t i,j;
	float f_start;
	float f_end;
	float f_blended;

	static float f_adc[NUM_ACTIVE_FILTS];
	float adc_lag[NUM_ACTIVE_FILTS];
	float freq_nudge[NUM_FILTS];

	float filter_out[NUM_FILTS][MONO_BUFSZ];

	uint8_t total_calc_filters=0;
	uint8_t nc_filter[12]={0,0,0,0,0,0,0,0,0,0,0,0};
	uint8_t nc_channel[12]={0,0,0,0,0,0,0,0,0,0,0,0};
	uint32_t need_calc=0;
	uint8_t k;

	static int8_t old_scale=-1;
	static int8_t old_scale_bank=-1;

	float var_q, inv_var_q, var_f, inv_var_f;
	register float tmp, fir, iir;
	float c0,c1,c2;
	float a0,a1,a2;
	uint8_t filter_num,channel_num;

	//~60us static, ~88us morphing



	//convert 16-bit pairs to 24-bits stuffed into 32-bit integers: 1.6us
	audio_convert_2x16_to_stereo24(DMA_xfer_BUFF_LEN, src, left_buffer, right_buffer);

	// Setup pot variables: 0.2us x 6 = 1.6us

	for (i=0;i<NUM_ACTIVE_FILTS;i++){

		//1st order LPF:
		env_in=((float)adc_buffer[i+2])/4096.0f;

		if(f_adc[i] < env_in) {
			f_adc[i] *= adc_lag_attack;
			f_adc[i] += (1.0f-adc_lag_attack)*env_in;
		} else {
			f_adc[i] *= adc_lag_decay;
			f_adc[i] += (1.0f-adc_lag_decay)*env_in;
		}
	}

	//Calculate assign_fade: 1.5us
	//Channel j should be the sum of filter_assign_table[j] (integer component) + assign_fade[j] (fractional component)
	//Thus if filter_assign_table[j]=2 and assign_fade[j]=0.25, the channel is essentially at f2.25 or 75% of f2 + 25% of f3.
	//Update assign_fade[] and inc/decrement filter_assign_table
	f_t=1.0/((adc_buffer[MORPH_ADC]+1.0)*1.5);

	for (j=0;j<NUM_ACTIVE_FILTS;j++){

		if ((dest_filter_assign_table[j] != filter_assign_table[j]) || assign_fade[j]!=0){
			assign_fade[j]+=rot_dir[j]*f_t;

			if (assign_fade[j]>=1){
				if (filter_assign_table[j]==(NUM_FILTS-1)) filter_assign_table[j]=0;
				else filter_assign_table[j]++;
				flag_update_LED_ring=1;

				if (dest_filter_assign_table[j] == filter_assign_table[j])
					assign_fade[j]=0;
				else
					assign_fade[j]-=1;

			}

			if (assign_fade[j]<=0){

				if (dest_filter_assign_table[j] == filter_assign_table[j]){
					assign_fade[j]=0;
				} else {
					assign_fade[j]+=1;
					if (filter_assign_table[j]==0) filter_assign_table[j]=NUM_FILTS-1;
					else filter_assign_table[j]--;

					flag_update_LED_ring=1;
				}

			}
			t=(filter_assign_table[j]+1) % NUM_FILTS;

			if (!(need_calc & (1<<t))){
				need_calc|=(1<<t);
				nc_channel[total_calc_filters]=j;
				nc_filter[total_calc_filters++]=(filter_assign_table[j]+1) % NUM_FILTS;
			}

		}
		if (!(need_calc & (1<<filter_assign_table[j]))){
			need_calc|=(1<<filter_assign_table[j]);
			nc_channel[total_calc_filters]=j;
			nc_filter[total_calc_filters++]=filter_assign_table[j];
		}

	}

	//Calc freq nudge and fill array: 1.2us
	for (i=0;i<NUM_FILTS;i++){
		if (i==filter_assign_table[0]) freq_nudge[i]=(float)adc_buffer[FREQNUDGE1_ADC]/4096.0;
		else if (i==filter_assign_table[5]) freq_nudge[i]=(float)adc_buffer[FREQNUDGE6_ADC]/4096.0;
		else freq_nudge[i]=0;
	}


	//Calculate all the needed filters: about 14us for 6, 29us for 12


	var_q=log_4096[adc_buffer[QVAL_ADC]];
	if (var_q<0.01) var_q=0.0;
	if (var_q>0.99) var_q=1.0;
	inv_var_q = 1.0-var_q;


	if (scale_bank==0){
		c=(float *)(filter_bpre_coefs_western_800Q+(scale*21));
		c_2q=(float *)(filter_bpre_coefs_western_2Q+(scale*21));

	} else if (scale_bank==1){
		c=(float *)(filter_bpre_coefs_indian_200Q+(scale*21));
		c_2q=(float *)(filter_bpre_coefs_indian_2Q+(scale*21));

	} else if (scale_bank==2){
		c=(float *)(filter_bpre_coefs_17ET_200Q+(scale*21));
		c_2q=(float *)(filter_bpre_coefs_17ET_2Q+(scale*21));

	} else if (scale_bank==3){
		c=(float *)(filter_bpre_coefs_gamma_800Q+(scale*21));
		c_2q=(float *)(filter_bpre_coefs_gamma_2Q+(scale*21));

	} else if (scale_bank==4){
		c=(float *)(filter_bpre_coefs_gammaspread1_800Q+(scale*21));
		c_2q=(float *)(filter_bpre_coefs_gammaspread1_2Q+(scale*21));
	}


	//a scale bank adds 7884 bytes

	if (old_scale!=scale || old_scale_bank!=scale_bank){
		old_scale=scale;
		old_scale_bank=scale_bank;
		for (i=0;i<NUM_FILTS;i++){
			buf[i][0]=0;
			buf[i][1]=0;
			buf[i][2]=0;
		}

	}

	DEBUG_ON(DEBUG2);

	/*
	 * float *c_hiq[6];
	 * float *c_loq[6];
	 *
	 * uint8_t scale[6];
	 * uint8_t scale_bank[6];
	 *
	 */

	for (j=0;j<total_calc_filters;j++){
		channel_num=nc_channel[j];
		filter_num=nc_filter[j];

		var_f=freq_nudge[filter_num];
		inv_var_f=1.0-var_f;

		a0=*(c_2q+filter_num*3+3)*var_f + *(c_2q+filter_num*3+0)*inv_var_f;
		a1=*(c_2q+filter_num*3+4)*var_f + *(c_2q+filter_num*3+1)*inv_var_f;
		a2=*(c_2q+filter_num*3+5)*var_f + *(c_2q+filter_num*3+2)*inv_var_f;

		c0=*(c+filter_num*3+3)*var_f + *(c+filter_num*3+0)*inv_var_f;
		c1=*(c+filter_num*3+4)*var_f + *(c+filter_num*3+1)*inv_var_f;
		c2=*(c+filter_num*3+5)*var_f + *(c+filter_num*3+2)*inv_var_f;

		c0=c0*var_q + a0*inv_var_q;
		c1=c1*var_q + a1*inv_var_q;
		c2=c2*var_q + a2*inv_var_q;

		for (i=0;i<MONO_BUFSZ;i++){

			tmp= buf[filter_num][0];
			buf[filter_num][0]=buf[filter_num][1];

			//Odd inputs go to odd filters numbers (1-20)
			if (filter_num & 1) iir=left_buffer[i] * c0;
			else iir=right_buffer[i] * c0;

			iir -= c1*tmp;
			fir= -tmp;
			iir -= c2*buf[filter_num][0];
			fir += iir;
			buf[filter_num][1]= iir;

			filter_out[filter_num][i]= fir;

		}
	}
	DEBUG_OFF(DEBUG2);



	for (i=0;i<MONO_BUFSZ;i++){

		filtered_buffer[i]=0;
		filtered_bufferR[i]=0;

		for (j=0;j<NUM_ACTIVE_FILTS;j++){

			//when blending, 1.5us/sample. When static, 0.7us/sample = 11-24us total
			if (assign_fade[j]==0){
				f_blended = filter_out[filter_assign_table[j]][i];
			} else {
				t=filter_assign_table[j]+1;
				if (t>=NUM_FILTS) t=0;
				f_blended = (filter_out[filter_assign_table[j]][i] * (1.0f-assign_fade[j])) + (filter_out[t][i] * assign_fade[j]);
			}



			//1.4us/sample = 22us total
			if (f_adc[j]>0.001){

				if (j & 1) s=filtered_buffer[i]+(f_blended*f_adc[j]);
				else s=filtered_bufferR[i]+(f_blended*f_adc[j]);

				asm("ssat %[dst], #32, %[src]" : [dst] "=r" (s) : [src] "r" (s));

				if (j & 1) filtered_buffer[i]=(int32_t)(s);
				else filtered_bufferR[i]=(int32_t)(s);

				if (((f_blended*f_adc[j]))>0x60000000)
					LED_ON((clip_led[j]));
				else
					LED_OFF((clip_led[j]));

			} else {
				LED_OFF((clip_led[j]));
			}


			//CALCULATE ENVELOPE OUTPUTS: 0.96 - 1.5us/sample = 16-24us total
			//1st order LPF:
			if (f_blended>0) env_in=f_blended;
			else env_in=-1.0*f_blended;

			if(envelope[j] < env_in) {
				envelope[j] *= gAttack;
				envelope[j] += (1.0-gAttack)*env_in;
			} else {
				envelope[j] *= gDecay;
				envelope[j] += (1.0-gDecay)*env_in;
			}
			//Pre-CV (input) or Post-CV (output)
			if (envout_mode==PRE) ENVOUT_PWM[j]=((uint32_t)envelope[j])>>16;
			else ENVOUT_PWM[j]=(uint32_t)(envelope[j]*f_adc[j])>>16;


		}

	}

	audio_convert_stereo24_to_2x16(DMA_xfer_BUFF_LEN, filtered_buffer, filtered_bufferR, dst); //1.5us
	DEBUG_OFF(DEBUG0);

}

/*
inline float filter_BpRe(register float val, uint8_t filter_num, float var1){
	DEBUG_ON(DEBUG2);

	   register float tmp, fir, iir;
	   float c0,c1,c2,inv_var1;
	   float d0,d1,d2;
	   uint8_t next_filter_num;

	   tmp= buf[filter_num][0];
	   buf[filter_num][0]=buf[filter_num][1];


	   if (var1<0.05){
		   c0=*(c+filter_num*3+0);
		   c1=*(c+filter_num*3+1);
		   c2=*(c+filter_num*3+2);

		   d0=filter_bpre_coefs_2Q_perfectfifth[filter_num][0];
		   d1=filter_bpre_coefs_2Q_perfectfifth[filter_num][1];
		   d2=filter_bpre_coefs_2Q_perfectfifth[filter_num][2];

		   var1=adc_buffer[QVAL_ADC]/4096.0;
		   inv_var1=1.0-var1;
		   c0=d0*var1 + c0*inv_var1;
		   c1=d1*var1 + c1*inv_var1;
		   c2=d2*var1 + c2*inv_var1;


	   } else {
			next_filter_num=filter_num+1;

			if (var1>0.95){
				   c0=*(c+next_filter_num*3+0);
				   c1=*(c+next_filter_num*3+1);
				   c2=*(c+next_filter_num*3+2);
			} else {
			   inv_var1=1.0-var1;
			   c0=*(c+next_filter_num*3+0)*var1 + *(c+filter_num*3+0)*inv_var1;
			   c1=*(c+next_filter_num*3+1)*var1 + *(c+filter_num*3+1)*inv_var1;
			   c2=*(c+next_filter_num*3+2)*var1 + *(c+filter_num*3+2)*inv_var1;
			}
	   }



	   iir= val * c0;
	   iir -= c1*tmp; fir= -tmp;
	   iir -= c2*buf[filter_num][0];

	   fir += iir;
	   buf[filter_num][1]= iir; val= fir;

	   DEBUG_OFF(DEBUG2);

	   return val;
	}
*/


/*
float filter_BpRe200(register float val, uint8_t filter_num, float var1){

DEBUG_ON(DEBUG2);
   static float buf[NUM_FILTS][2];
   register float tmp, fir, iir;
   float c0,c1,c2,inv_var1;
   uint8_t next_filter_num;

   tmp= buf[filter_num][0];
   buf[filter_num][0]=buf[filter_num][1];

   if (var1<0.05){
	   c0=filter_bpre_coefs_200Q[filter_num][0];
	   c1=filter_bpre_coefs_200Q[filter_num][1];
	   c2=filter_bpre_coefs_200Q[filter_num][2];
   } else {
		next_filter_num=filter_num+1;

		if (var1>0.95){
		   c0=filter_bpre_coefs_200Q[next_filter_num][0];
		   c1=filter_bpre_coefs_200Q[next_filter_num][1];
		   c2=filter_bpre_coefs_200Q[next_filter_num][2];
		} else {
		   inv_var1=1.0-var1;
		   c0=filter_bpre_coefs_200Q[next_filter_num][0]*var1 + filter_bpre_coefs_200Q[filter_num][0]*inv_var1;
		   c1=filter_bpre_coefs_200Q[next_filter_num][1]*var1 + filter_bpre_coefs_200Q[filter_num][1]*inv_var1;
		   c2=filter_bpre_coefs_200Q[next_filter_num][2]*var1 + filter_bpre_coefs_200Q[filter_num][2]*inv_var1;
		}
   }

   iir= val * c0;
   iir -= c1*tmp; fir= -tmp;
   iir -= c2*buf[filter_num][0];

   fir += iir;
   buf[filter_num][1]= iir; val= fir;

   DEBUG_OFF(DEBUG2);

   return val;
}
*/

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

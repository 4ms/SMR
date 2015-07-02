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
#include "exp_4096.h"

float adc_lag_attack=0;
float adc_lag_decay=0;

extern float log_4096[4096];

extern uint8_t note[NUM_CHANNELS];
extern uint8_t scale[NUM_CHANNELS];
extern uint8_t scale_bank[NUM_CHANNELS];
extern uint16_t rotate_to_next_scale;

extern uint8_t g_error;

extern uint32_t env_prepost_mode;

extern uint32_t ENVOUT_PWM[NUM_CHANNELS];
extern const uint32_t slider_led[6];

extern uint8_t lock[NUM_CHANNELS];
extern uint8_t lock_pos[NUM_CHANNELS];

extern uint8_t flag_update_LED_ring;

extern __IO uint16_t adc_buffer[NUM_ADCS];
extern __IO uint16_t potadc_buffer[NUM_ADC3S];

extern uint32_t qval[NUM_CHANNELS];

//extern float spectral_readout[NUM_FILTS];

//extern uint8_t strike;

extern float freq_nudge[NUM_CHANNELS];

extern int8_t motion_rotate[NUM_CHANNELS];
extern int8_t motion_spread[NUM_CHANNELS];
extern int8_t motion_notejump[NUM_CHANNELS];
extern int8_t motion_scale[NUM_CHANNELS];

extern int8_t motion_dst_note[NUM_CHANNELS];
extern int8_t motion_dst_scale[NUM_CHANNELS];
extern int8_t motion_dst_bank[NUM_CHANNELS];

float motion_morphpos[NUM_CHANNELS]={0.0,0.0,0.0,0.0,0.0,0.0};

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


/*

inline float calc_filter_level(uint8_t chan){
	float t;
	//t=((float)(adc_buffer[chan+LEVEL_ADC_BASE] + potadc_buffer[chan+SLIDER_ADC_BASE]))/4096.0f;
	t=((float)(adc_buffer[chan+LEVEL_ADC_BASE])/4096.0)  *  ((float)(potadc_buffer[chan+SLIDER_ADC_BASE])/4096.0f);
	if (t>1.0f) return (1.0f);
	else return (t);
}
*/

float *c_hiq[6];
float *c_loq[6];
float buf[NUM_CHANNELS][NUMSCALES][NUM_FILTS][2];
float f_adc[NUM_CHANNELS];

uint32_t env_trigout[NUM_CHANNELS];

void process_audio_block(int16_t *src, int16_t *dst, uint16_t ht)
{
	DEBUGA_ON(DEBUG0);
	uint16_t chan;
	uint16_t test_chan;
	int32_t test_dst[NUM_CHANNELS]={99,99,99,99,99,99};

	int16_t i,j;
	uint8_t k;
	int32_t s;
	uint32_t t;

	static float envelope[NUM_CHANNELS];
	float env_in;

	float f_morph;

	float f_start;
	float f_end;
	float f_blended;

	float *ff;

	float adc_lag[NUM_CHANNELS];

	float filter_out[NUM_FILTS][MONO_BUFSZ];

	static uint8_t old_scale[NUM_CHANNELS]={-1,-1,-1,-1,-1,-1};
	static uint8_t old_scale_bank[NUM_CHANNELS]={-1,-1,-1,-1,-1,-1};

	static float f_slider_pwm=0;

	uint8_t is_distinct;

	float var_q, inv_var_q, var_f, inv_var_f;
	register float tmp, fir, iir;
	float c0,c1,c2;
	float a0,a1,a2;
	uint8_t filter_num,channel_num;
	uint8_t scale_num;

	uint8_t nudge_filter_num;

	//~60us static, ~88us morphing


	// Convert 16-bit pairs to 24-bits stuffed into 32-bit integers: 1.6us
	audio_convert_2x16_to_stereo24(DMA_xfer_BUFF_LEN, src, left_buffer, right_buffer);


	// Calculate the CV/Slider level and display it on the slider LEDs (if enabled): 0.2us x 6 = 1.6us
	// f_slider_pwm is the PWM counter for slider LED brightness
	f_slider_pwm+=0.02;
	if (f_slider_pwm>1.0) f_slider_pwm=0.0;

	for (i=0;i<NUM_CHANNELS;i++){

		//1st order LPF:
		//env_in=((float)(adc_buffer[chan+LEVEL_ADC_BASE])/4096.0)  *  ((float)(potadc_buffer[chan+SLIDER_ADC_BASE])/4096.0f);
		env_in=((float)(adc_buffer[chan+LEVEL_ADC_BASE])/4096.0)  +  ((float)(potadc_buffer[chan+SLIDER_ADC_BASE])/4096.0f);
		if (env_in>1.0f) env_in=1.0f;


		if(f_adc[i] < env_in) {
			f_adc[i] *= adc_lag_attack;
			f_adc[i] += (1.0f-adc_lag_attack)*env_in;
		} else {
			f_adc[i] *= adc_lag_decay;
			f_adc[i] += (1.0f-adc_lag_decay)*env_in;
		}

		// Display f_adc[] as an analog value on the LEDs using PWM
		if (SLIDER_LEDS==SHOW_LEVEL){
			if (f_adc[i]>f_slider_pwm || f_slider_pwm==0.00)
				LED_SLIDER_ON((slider_led[i]));
			else
				LED_SLIDER_OFF((slider_led[i]));
		}
	}


	//f_morph is how fast we morph (cross-fade)
	f_morph=exp_4096[(adc_buffer[MORPH_ADC])];

	//Use:
	//motion_rotate[chan]: incremental (rotation) motion within a scale. E.g. +3 means rotate +1 three times.
	//  --rotate_to_next_scale to determine if a rotinc_mf_motion also changes the scale as we pass the 19/0 threshold
	//
	//motion_spread[chan]: is incremental spread rotation. Works just like motion_rotate except rotating the opposite direction does not cancel out a spread motion
	//
	//motion_notejump[chan]: instant jumping motion within a scale. E.g. +3 means morph to a position 3 rotations from the current position
	//
	//motion_scale[chan]: motion from one scale to another
	//
	//(motion_bank[chan]): motion from one bank to another
	//
	//To produce:
	//  motion_dst_note[chan], motion_dst_scale[chan], motion_dst_bank[chan]
	//Which we crossfade with:
	//  note[chan], scale[chan], scale_bank[chan]
	//
	//motion_morphpos (0.0-1.0) tells us where the current output is within the crossfade
	//
	// When an event is processed (rotatry knob, trig jack, CV jack) we set the motion variables (do not consider if this places us on a locked filter)
	//


	//if (note[chan]!=motion_dst_note[chan] || scale[chan]!=motion_dst_scale[chan] || scale_bank[chan]!=motion_dst_bank[chan]){
	//
	//}


	for (chan=0;chan<NUM_CHANNELS;chan++){

		//if morph is happening, continue it
		if (motion_morphpos[chan]>0)
			motion_morphpos[chan] += f_morph;

		//if morph has reached the end, shift our present position to the dst
		if (motion_morphpos[chan]>=1.0){
			note[chan] = motion_dst_note[chan];
			scale[chan] = motion_dst_scale[chan];
			//scale_bank[chan] = motion_dst_bank[chan];
			motion_morphpos[chan]=0.0;

			flag_update_LED_ring=1;
		}

		//if motion is not happening, check to see if there's something queued
		if (motion_morphpos[chan]==0.0){
			/*if (motion_scale[chan]!=0){

			} else if (motion_notejump[chan] != 0){

			} else */

			//Clockwise Spread, or no Spread and Clockwise Rotation
			if (motion_spread[chan] > 0 || (!is_spreading() && motion_rotate[chan] > 0)) {

				//Start the motion morph
				motion_morphpos[chan] = f_morph;

				//If we're rotating (not spreading), then clear the motion from the rotate queue.
				//We just clear one rotate motion, even if we have several is_distinct conflicts, since we don't have a particular destination
				if (!is_spreading() && motion_rotate[chan] > 0) motion_rotate[chan]--;


				// Shift the destination CW, wrapping it around
				is_distinct=0;
				while (!is_distinct){

					//Increment circularly
					if (motion_dst_note[chan]>=(NUM_FILTS-1)){
						motion_dst_note[chan]=0;

						// If scale rotation is on, increment the scale, wrapping it around
						if (rotate_to_next_scale) motion_dst_scale[chan]=(motion_dst_scale[chan]+1) % NUMSCALES;
					}
					else
						motion_dst_note[chan]++;

					//clear the motion from the queue
					if (motion_spread[chan] > 0) motion_spread[chan]--;


					//Check that it's not already taken by a locked channel, channel with a higher priority, or a non-moving channel
					for (is_distinct=1,test_chan=0;test_chan<NUM_CHANNELS;test_chan++){
						if (chan!=test_chan && (motion_dst_note[chan]==motion_dst_note[test_chan]) && (lock[test_chan]==1 || test_chan<chan || (motion_rotate[test_chan]==0 && motion_spread[test_chan]==0)))
							is_distinct=0;
					}
				}
			} else

			//Counter-clockwise Spread, or no Spread and CCW Rotation
			if (motion_spread[chan]<0 || (!is_spreading() && motion_rotate[chan] < 0)){

				//Start the motion morph
				motion_morphpos[chan] = f_morph;

				//If we're rotating (not spreading), then clear the motion from the rotate queue.
				//We just clear one rotate motion, even if we have several is_distinct conflicts, since we don't have a particular destination
				if (!is_spreading() && motion_rotate[chan] < 0) motion_rotate[chan]++;

				// Shift the destination CCW, wrapping it around and repeating until we find a distinct value
				is_distinct=0;
				while (!is_distinct){

					//If we're spreading (regardless of rotation), then clear the motion from the queue each time we decrement motion_dst_note,
					//since we have a particular destination (final_dest = starting_note + motion_spread)
					if (motion_spread[chan] < 0) motion_spread[chan]++;

					//Decrement circularly
					if (motion_dst_note[chan]==0) {
						motion_dst_note[chan] = NUM_FILTS-1;

						// If scale rotation is on, decrement the scale, wrapping it around
						if (rotate_to_next_scale){
							if (motion_dst_scale[chan]==0) motion_dst_scale[chan]=NUMSCALES-1;
							else motion_dst_scale[chan]--;
						}
					}
					else
						motion_dst_note[chan]--;

					//Check that it's not already taken by a locked channel, channel with a higher priority, or a non-moving channel
					for (is_distinct=1,test_chan=0;test_chan<NUM_CHANNELS;test_chan++){
						if (chan!=test_chan && (motion_dst_note[chan]==motion_dst_note[test_chan]) && (lock[test_chan]==1 || test_chan<chan || (motion_rotate[test_chan]==0 && motion_spread[test_chan]==0)))
							is_distinct=0;
					}
				}
			}
		}
	}



/*
	for (j=0;j<NUM_CHANNELS;j++){

		// Fade from scale to dest_scale, in steps of f_morph.
		// scale_fade ranges from 0 to 1 to indicate the crossfade position

		// We either are doing a scale_fade or a rotate/spread assign_fade (we don't want to calculate both vectors at the same time)

		// Scale fade:

		if (assign_fade[j]<f_morph && assign_fade[j]>(-1.0*f_morph) && scale_dir[j]!=0){ //rot/spread is done and there's a pending scale_dir

			if (scale_dir[j]!=0){
				scale_fade[j]+=f_morph;
			}

			if (scale_fade[j]>=1.0){
				scale[j]=dest_scale[j];
				scale_fade[j]=0;
				scale_dir[j]=0;
			}

		} else {

		//Assign Fade:

		if ((dest_filter_assign_table[j] != filter_assign_table[j]) || assign_fade[j]!=0 || (dest_scale[j]!=scale[j])  || rot_dir[j]!=0)
			{

				//spread takes precedence over rot

				if (spread_dir[j]!=0.0){
					assign_fade[j]+=spread_dir[j]*f_morph;
					rot_dir[j]=0;
				} else {
					assign_fade[j]+=rot_dir[j]*f_morph;
				}

				if (assign_fade[j]>=1){
					flag_update_LED_ring=1;

					filter_assign_table[j]=(filter_assign_table[j]+1) % NUM_FILTS;

					if (filter_assign_table[j]==0 && dest_scale[j]!=scale[j])
						scale[j]=dest_scale[j];



					if (dest_filter_assign_table[j] == filter_assign_table[j] && dest_scale[j]==scale[j])
					{
						assign_fade[j]=0;
						is_fading_down_across_scales[j]=0;
						rot_dir[j]=0;
						spread_dir[j]=0;

					} else{
						assign_fade[j]-=1;
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
						if (filter_assign_table[j]==0)
							filter_assign_table[j]=NUM_FILTS-1;
						else
							filter_assign_table[j]--;

						if (filter_assign_table[j]==(NUM_FILTS-1) && dest_scale[j]!=scale[j]){
							scale[j]=dest_scale[j];
							is_fading_down_across_scales[j]=1;
						}

						flag_update_LED_ring=1;
					}

				}

			}
		}
	}
*/

	//Determine the coef tables we're using for the active filters (Lo-Q and Hi-Q) for each channel
	//Also clear the buf[] history if we changed scales or banks, so we don't get artifacts
	for (i=0;i<NUM_CHANNELS;i++){

		//range check scale_bank and scale
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

			} else if (scale_bank[i]==6){
				c_hiq[i]=(float *)(filter_bpre_coefs_twelvetone_800Q);
				c_loq[i]=(float *)(filter_bpre_coefs_twelvetone_2Q);

			} else if (scale_bank[i]==7){
				c_hiq[i]=(float *)(filter_bpre_coefs_diatonic_800Q);
				c_loq[i]=(float *)(filter_bpre_coefs_diatonic_2Q);
			}

		}
	}



	//Calculate all the needed filters: about 14us for 6, 29us for 12
	//DEBUGA_ON(DEBUG2);

	//Calculate filter_out[]
	//filter_out[0-5] are the note[]/scale[]/scale_bank[] filters. filter_out[6-11] are the morph destination values

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
				filter_num=motion_dst_note[channel_num];
				scale_num=motion_dst_scale[channel_num];
			}

			var_f=freq_nudge[channel_num];
			if (var_f<0.002) var_f=0.0;
			if (var_f>0.998) var_f=1.0;
			inv_var_f=1.0-var_f;

			//Freq vector
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

			if (motion_morphpos[j]==0){
				f_blended = filter_out[j][i];
			} else {
				//Morph fade
				f_blended = (filter_out[j][i] * (1.0f-motion_morphpos[j])) + (filter_out[j+NUM_CHANNELS][i] * motion_morphpos[j]);
			}




			//1.4us/sample = 22us total
			if (f_adc[j]>0.004){

				if (j & 1) s=filtered_buffer[i]+(f_blended*f_adc[j]);
				else s=filtered_bufferR[i]+(f_blended*f_adc[j]);

				asm("ssat %[dst], #32, %[src]" : [dst] "=r" (s) : [src] "r" (s));

				if (j & 1) filtered_buffer[i]=(int32_t)(s);
				else filtered_bufferR[i]=(int32_t)(s);

				//Check for audio clipping, and light the clip LEDs (slider LEDs, or the audio input LEDs)
				if (((f_blended*f_adc[j]))>0x60000000){
					if (SLIDER_LEDS==SHOW_CLIPPING) LED_SLIDER_ON((slider_led[j]));
					else { if (j&1) LED_CLIPL_ON; else LED_CLIPR_ON; }
				} else {
					if (SLIDER_LEDS==SHOW_CLIPPING) LED_SLIDER_OFF((slider_led[j]));
					else { if (j&1) LED_CLIPL_OFF; else LED_CLIPR_OFF; }
				}

			} else {
				//Turn off the clip lights (slider LEDs, or the audio input LEDs)
				if (SLIDER_LEDS==SHOW_CLIPPING) LED_SLIDER_OFF((slider_led[j]));
				else { if (j&1) LED_CLIPL_OFF; else LED_CLIPR_OFF; }
			}


			//CALCULATE ENVELOPE OUTPUTS: 0.96 - 1.5us/sample = 16-24us total
			//1st order LPF:
			if (f_blended>0) env_in=f_blended;
			else env_in=-1.0*f_blended;


			if (ENVSPEEDSLOW || ENVSPEEDFAST){
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
			} else { //trigger mode
				//Pre-CV (input) or Post-CV (output)
				if (env_prepost_mode!=PRE) env_in=env_in*f_adc[j];

				if (env_trigout[j]){
					env_trigout[j]--;
					if (env_trigout[j]==0) ENVOUT_PWM[j]=1;
				} else {
					if (((uint32_t)env_in)>(1<<27)) {
						env_trigout[j]=10000; //10/96 is about 10ms
						ENVOUT_PWM[j]=4090;
					} else {
						ENVOUT_PWM[j]=1;
					}
				}
		
			}



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

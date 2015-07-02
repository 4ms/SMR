/*
 * params.c
 *
 *  Created on: Jun 17, 2015
 *      Author: design
 */


void update_params(void){
	t_f=(float)(adc_buffer[FREQNUDGE1_ADC]+adc_buffer[FM_135_ADC])/4096.0;
	if (t_f>1.0) t_f=1.0;

	f_freq_odds *= adc_lpf;
	f_freq_odds += (1.0f-adc_lpf)*t_f;

	freq_nudge[0]=t_f;

	if (mod_mode_135==135){
		freq_nudge[2]=t_f;
		freq_nudge[4]=t_f;
	} else {
		freq_nudge[2]=0.0;
		freq_nudge[4]=0.0;
	}

	t_f=(float)(adc_buffer[FREQNUDGE6_ADC]+adc_buffer[FM_246_ADC])/4096.0;
	if (t_f>1.0) t_f=1.0;

	f_freq_evens *= adc_lpf;
	f_freq_evens += (1.0f-adc_lpf)*t_f;

	if (mod_mode_246==246){
		freq_nudge[1]=t_f;
		freq_nudge[3]=t_f;
	} else {
		freq_nudge[1]=0.0;
		freq_nudge[3]=0.0;
	}

	freq_nudge[5]=t_f;
}

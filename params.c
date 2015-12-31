/*
 * params.c - Parameters
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

#include "globals.h"
#include "params.h"
#include "dig_inouts.h"
#include "exp_1voct.h"
#include "system_mode.h"
#include "rotary.h"

extern __IO uint16_t adc_buffer[NUM_ADCS];
extern __IO uint16_t potadc_buffer[NUM_ADC3S];

extern uint8_t select_colors_mode;

enum Filter_Types filter_type=MAXQ;

extern enum UI_Modes ui_mode;

extern uint8_t editscale_notelocked;
extern uint8_t editscale_tracklocked;

uint16_t old_adc_buffer[NUM_ADCS];
uint16_t old_potadc_buffer[NUM_ADC3S];

extern uint32_t rotary_state;

float trackcomp[NUM_CHANNELS]={1.0,1.0};
int16_t trackoffset[NUM_CHANNELS]={0,0};


//FREQ NUDGE/LOCK JACKS
float freq_nudge[NUM_CHANNELS];
float freq_shift[NUM_CHANNELS];
uint16_t mod_mode_135;
uint16_t mod_mode_246;

extern uint8_t do_LOCK135;
extern uint8_t do_LOCK246;

//LOCK BUTTONS
uint8_t lock[NUM_CHANNELS];
uint8_t lock_pressed[NUM_CHANNELS];
uint8_t lock_up[NUM_CHANNELS];
uint32_t lock_down[NUM_CHANNELS];
uint8_t already_handled_lock_release[NUM_CHANNELS];

//ENV OUTS
uint32_t env_prepost_mode;
enum Env_Out_Modes env_track_mode;
float envspeed_attack, envspeed_decay;

//ROTATE SCALE
uint16_t rotate_to_next_scale;

//CHANNEL LEVELS/SLEW
float channel_level[NUM_CHANNELS]={0,0,0,0,0,0};
float LEVEL_LPF_ATTACK=0;
float LEVEL_LPF_DECAY=0;


//Q POT AND CV
uint32_t qval[NUM_CHANNELS];
uint32_t qvalcv, qvalpot;
uint8_t q_locked[NUM_CHANNELS]={0,0,0,0,0,0};
uint8_t user_turned_Q_pot=0;

//Filters
uint8_t note[NUM_CHANNELS];
uint8_t scale[NUM_CHANNELS];
uint8_t scale_bank[NUM_CHANNELS];

//Motion
extern int8_t motion_spread_dest[NUM_CHANNELS];
extern int8_t motion_spread_dir[NUM_CHANNELS];
extern int8_t motion_scale_dest[NUM_CHANNELS];
extern int8_t motion_fadeto_note[NUM_CHANNELS];
extern int8_t motion_fadeto_scale[NUM_CHANNELS];
extern int8_t motion_rotate;
extern int8_t motion_scalecv_overage[NUM_CHANNELS];
extern int8_t motion_notejump;
extern float motion_morphpos[NUM_CHANNELS];

extern float rot_dir[6];

extern int8_t spread;

//Scale CV
uint8_t scale_cv[NUM_CHANNELS];

//Scale bank
uint8_t hover_scale_bank=0;
int16_t change_scale_mode=0;

extern uint8_t cur_param_bank;
extern uint8_t cur_colsch;

void set_default_param_values(void){
	uint8_t i;

	//Set default parameter values
	for (i=0;i<NUM_CHANNELS;i++){
		note[i]=i+5;
		scale[i]=6;
		motion_fadeto_scale[i]=scale[i];
		motion_scale_dest[i]=scale[i];
		scale_bank[i]=0;
		rot_dir[i]=0;
		motion_spread_dir[i]=0;
		motion_spread_dest[i]=note[i];
		motion_fadeto_note[i]=note[i];

		motion_morphpos[i]=0;
		freq_shift[i]=0;
		motion_scalecv_overage[i]=0;
	}
	motion_notejump=0;
	motion_rotate=0;
	filter_type=MAXQ;

	trackcomp[0]=1.0;
	trackcomp[1]=1.0;
	trackoffset[0]=0;
	trackoffset[1]=0;
}

void param_read_freq_nudge(void){
	float t_fo, t_fe;
	static float f_nudge_odds=1, f_nudge_evens=1;
	static float f_shift_odds=1, f_shift_evens=1;

	int32_t freq_jack_cv;

	//With the Maxq filter, the Freq Nudge pot alone adjusts the "nudge", and the CV jack is 1V/oct shift
	//With the BpRe filter, the Freq Nudge pot plus CV jack adjusts the "nudge", and there is no 1V/oct shift
	if (filter_type==MAXQ){
		t_fo=(float)(adc_buffer[FREQNUDGE1_ADC])/4096.0;
		t_fe=(float)(adc_buffer[FREQNUDGE6_ADC])/4096.0;

		if (trackcomp[0]<0.5 || trackcomp[0]>2.0) trackcomp[0]=1.0; //sanity check

		freq_jack_cv = (adc_buffer[FREQCV1_ADC] + trackoffset[0]) * trackcomp[0];
		if (freq_jack_cv<0) freq_jack_cv=0;
		if (freq_jack_cv>4095) freq_jack_cv=4095;

		f_shift_odds *= FREQCV_LPF;
		f_shift_odds += (1.0f-FREQCV_LPF)*(float)(exp_1voct[freq_jack_cv]) ;

		if (trackcomp[1]<0.5 || trackcomp[1]>2.0) trackcomp[1]=1.0; //sanity check

		freq_jack_cv = (adc_buffer[FREQCV6_ADC] + trackoffset[1]) * trackcomp[1];
		if (freq_jack_cv<0) freq_jack_cv=0;
		if (freq_jack_cv>4095) freq_jack_cv=4095;

		f_shift_evens *= FREQCV_LPF;
		f_shift_evens += (1.0f-FREQCV_LPF)*(float)(exp_1voct[freq_jack_cv]);

	}else{

		t_fo=(float)(adc_buffer[FREQNUDGE1_ADC]+adc_buffer[FREQCV1_ADC])/4096.0;
		if (t_fo>1.0) t_fo=1.0;

		t_fe=(float)(adc_buffer[FREQNUDGE6_ADC]+adc_buffer[FREQCV6_ADC])/4096.0;
		if (t_fe>1.0) t_fe=1.0;

		f_shift_odds=1.0;
		f_shift_evens=1.0;
	}

	f_nudge_odds *= FREQNUDGE_LPF;
	f_nudge_odds += (1.0f-FREQNUDGE_LPF)*t_fo;

	f_nudge_evens *= FREQNUDGE_LPF;
	f_nudge_evens += (1.0f-FREQNUDGE_LPF)*t_fe;

	if (!lock[0]){
		freq_nudge[0]=f_nudge_odds;
	}
	freq_shift[0]=f_shift_odds;

	if (mod_mode_135==135){
		if (!lock[2]){
			freq_nudge[2]=f_nudge_odds;
		}
		freq_shift[2]=f_shift_odds;

		if (!lock[4]){
			freq_nudge[4]=f_nudge_odds;
		}
		freq_shift[4]=f_shift_odds;
	} else {
		if (!lock[2]){
			freq_nudge[2]=0.0;
		}
		freq_shift[2]=1.0;

		if (!lock[4]){
			freq_nudge[4]=0.0;
		}
		freq_shift[4]=1.0;
	}

	if (!lock[5]){
		freq_nudge[5]=f_nudge_evens;
	}
	freq_shift[5]=f_shift_evens;

	if (mod_mode_246==246){
		if (!lock[1]){
			freq_nudge[1]=f_nudge_evens;
		}
		freq_shift[1]=f_shift_evens;

		if (!lock[3]){
			freq_nudge[3]=f_nudge_evens;
		}
		freq_shift[3]=f_shift_evens;
	} else {
		if (!lock[1]){
			freq_nudge[1]=0.0;
		}
		freq_shift[1]=1.0;

		if (!lock[3]){
			freq_nudge[3]=0.0;
		}
		freq_shift[3]=1.0;
	}
}


void param_read_channel_level(void){
	float level_lpf;
	uint8_t i;
	uint16_t t;

	if (ui_mode==EDIT_SCALES){
		if (env_track_mode!=ENV_SLOW) channel_level[0]=1.0;
		else channel_level[0]=0.0;

		if (env_track_mode!=ENV_FAST) channel_level[5]=1.0;
		else channel_level[5]=0.0;

		channel_level[1]=0.0;
		channel_level[2]=0.0;
		channel_level[3]=0.0;
		channel_level[4]=0.0;

	} else {

		for (i=0;i<NUM_CHANNELS;i++){

			//This is to be used if the sliders provide offset to the CV (hardware should be modified to remove Vref from switch tab of CV jacks)
			//level_lpf=((float)(adc_buffer[i+LEVEL_ADC_BASE])/4096.0)  +  ((float)(potadc_buffer[i+SLIDER_ADC_BASE])/4096.0f);
			//if (level_lpf>1.0f) level_lpf=1.0f;


			//This is to be used if the sliders attenuate the CV:

			//Account for error on faceplate that doesn't allow slider to go to zero
			t=potadc_buffer[i+SLIDER_ADC_BASE];
			if (t<20)
				t=0;
			else
				t=t-20;

			level_lpf=((float)(adc_buffer[i+LEVEL_ADC_BASE])/4096.0) *  (float)(t)/4096.0;

			if (level_lpf<=0.007) level_lpf=0.0;

			if(channel_level[i] < level_lpf) {
				channel_level[i] *= LEVEL_LPF_ATTACK;
				channel_level[i] += (1.0f-LEVEL_LPF_ATTACK)*level_lpf;
			} else {
				channel_level[i] *= LEVEL_LPF_DECAY;
				channel_level[i] += (1.0f-LEVEL_LPF_DECAY)*level_lpf;
			}

			//if (channel_level[i]<0.0003) channel_level[i]=0.0;
		}
	}
}


void param_read_q(void){
	int32_t t, i, num_locked;
	static uint32_t qpot_lpf=0;
	static uint32_t old_qpot_lpf=0xFFFF;
	static poll_ctr=0;

	if (poll_ctr++>10){poll_ctr=0;

		//Check jack
		t=adc_buffer[QVAL_ADC];

		if (diff(t,old_adc_buffer[QVAL_ADC])>15){
			old_adc_buffer[QVAL_ADC]=adc_buffer[QVAL_ADC];
			qvalcv=adc_buffer[QVAL_ADC];
		}


		//Check pot
		t = potadc_buffer[QPOT_ADC];

	//	qpot_lpf *= QPOT_LPF;
	//	qpot_lpf += (1.0f-QPOT_LPF)*t;

		qpot_lpf=t;

		num_locked=0;

		if (diff(qpot_lpf, old_qpot_lpf) > QPOT_MIN_CHANGE){
			old_qpot_lpf=qpot_lpf;

			if (ui_mode==PLAY){
				for (i=0;i<6;i++){
					if (lock_pressed[i]){ //if lock button is being held down, then q_lock the channel and assign its qval
						q_locked[i]=1;
						user_turned_Q_pot=1;

						qval[i]=qpot_lpf;
						num_locked++;
					}
				}
			}

			//otherwise, if no lock buttons were held down, then change the qvalpot (which effects all non-q_locked channels)
			//if (!j) qvalpot=qpot_lpf;
		}

		if (!num_locked) qvalpot=qpot_lpf;

		for (i=0;i<NUM_CHANNELS;i++){
			if (!q_locked[i]){
				qval[i]=qvalcv + qvalpot;
				if (qval[i]>4095) qval[i]=4095;
			}
		}
	}
}

void param_read_switches(void){
	static uint32_t old_cvlag=0xFFFF;
	uint32_t lag_val;
	float t_LEVEL_LPF_DECAY, t_LEVEL_LPF_ATTACK;


/*** Read Switches ***/

	//PRE|POST switch
	if (ENV_MODE){
		env_prepost_mode=0;
	} else {
		env_prepost_mode=1;
	}


	if (MOD246){
		mod_mode_246=6;
	} else {
		mod_mode_246=246;
	}

	if (MOD135){
		mod_mode_135=1;
	} else {
		mod_mode_135=135;
	}

	if (RANGE_MODE){
		rotate_to_next_scale=1;
	} else {
		rotate_to_next_scale=0;
	}

	//float ga = exp(-1.0f/(SampleRate*AttackTimeInSecond));
	if (ENVSPEEDFAST) {
		env_track_mode=ENV_FAST;
		//envspeed_attack=0.0;
		//envspeed_decay=0.0;
		envspeed_attack=0.9990;
		envspeed_decay=0.9991;
	} else {
		if (ENVSPEEDSLOW){
			env_track_mode=ENV_SLOW;
			envspeed_attack=0.9995;
			envspeed_decay=0.9999;
		} else { //trigger
			env_track_mode=ENV_TRIG;
			envspeed_attack=0.0;
			envspeed_decay=0.0;
		}
	}

	lag_val=CVLAG;
	if (old_cvlag!=lag_val){
		old_cvlag=lag_val;

		if (lag_val){
			lag_val=adc_buffer[MORPH_ADC];
			if (lag_val<200) lag_val=200;

			t_LEVEL_LPF_ATTACK=	1.0 - (1.0/((lag_val)*0.1));
			t_LEVEL_LPF_DECAY=	1.0 - (1.0/((lag_val)*0.25));

			if (t_LEVEL_LPF_ATTACK<0)	LEVEL_LPF_ATTACK=0;
			else LEVEL_LPF_ATTACK = t_LEVEL_LPF_ATTACK;
			if (t_LEVEL_LPF_DECAY<0)	LEVEL_LPF_DECAY=0;
			else LEVEL_LPF_DECAY = t_LEVEL_LPF_DECAY;

		}else{
			LEVEL_LPF_ATTACK=LAG_ATTACK_MIN_LPF;
			LEVEL_LPF_DECAY=LAG_DECAY_MIN_LPF;
		}
	}

}

inline uint8_t num_locks_pressed(void){
	uint8_t i,j;
	for (i=0,j=0;i<NUM_CHANNELS;i++){
		if (lock_pressed[i]) j++;
	}
	return j;
}



void process_lock_buttons(void){
	uint8_t i;

	for (i=0;i<6;i++){
		if (LOCKBUTTON(i)){
			lock_up[i]=1;
			if (lock_down[i]!=0
				&& lock_down[i]!=0xFFFFFFFF)  //don't wrap our counter!
				lock_down[i]++;

			if (lock_down[i]==5){ //first time we notice lock button is solidly down...
				lock_pressed[i]=1;
				user_turned_Q_pot=0;
				already_handled_lock_release[i]=0;
			}

			if (ui_mode==PLAY){
				//check to see if it's been held down for a while, and the user hasn't turned the Q pot
				//if so, then we should unlock immediately, but not unlock the q_lock
				if (lock_down[i]>LOCK_BUTTON_QUNLOCK_HOLD_TIME && lock[i] && !user_turned_Q_pot) {
					lock[i]=0;
					LOCKLED_OFF(i);
					already_handled_lock_release[i]=1; //set this flag so that we don't do anything when the button is released
					lock_down[i]=0; //stop checking this button until it's released
				}
			}
			if (ui_mode==SELECT_PARAMS){
				if (lock_down[i]>LOCK_BUTTON_LONG_HOLD_TIME){

					if (num_locks_pressed() == 6 && ROTARY_SW){
						factory_reset();
						exit_system_mode(0); //do not restore lock[] because factory reset clears them
						while (ROTARY_SW) {;}
						already_handled_lock_release[0]=1;already_handled_lock_release[1]=1;already_handled_lock_release[2]=1;
						already_handled_lock_release[3]=1;already_handled_lock_release[4]=1;already_handled_lock_release[5]=1;

					} else {
						exit_system_mode(1); //restore lock[] so that it gets saved
						save_param_bank(i);
						already_handled_lock_release[i]=1;
						lock_down[i]=0; //stop checking this button until it's released

					}
				}
			}

		} else {
			if (lock_up[i]!=0) lock_up[i]++;
			if (lock_up[i]>5){ lock_up[i]=0;
				//Handle button release

				lock_pressed[i]=0;

				if (ui_mode==PLAY){
					if (!user_turned_Q_pot && !already_handled_lock_release[i]){ //only change lock state if user did not do a q_lock

						if (lock[i]==0){
							lock[i]=1;
							//note[i]=motion_fadeto_note[i];
							//motion_spread_dest[i]=note[i];
							motion_spread_dir[i]=0;
							LOCKLED_ON(i);
						}
						else {
							lock[i]=0;
							q_locked[i]=0;
							LOCKLED_OFF(i);
						}
					}
				}

				if (ui_mode==EDIT_COLORS){
					if (!already_handled_lock_release[i]){
						if (lock[i]==0){
							lock[i]=1;
							LOCKLED_ON(i);
						}
						else {
							lock[i]=0;
							LOCKLED_OFF(i);
						}
					}
				}
				if (ui_mode==EDIT_SCALES){
					if (i<4)
						editscale_notelocked=1-editscale_notelocked;
					else
						editscale_tracklocked=1-editscale_tracklocked;

				}

				if (ui_mode==SELECT_PARAMS){
					if (!already_handled_lock_release[i]){
						load_param_bank(i);
						exit_system_mode(0); //do not restore lock[] because we are loading new lock settings

					}
				}
			}

			lock_down[i]=1;

		}
	}

}
void process_lock_jacks(void){

	if (do_LOCK135){
		do_LOCK135=0;

		lock[0]=1-lock[0];
		if (lock[0]) LOCKLED_ON(0);
		else LOCKLED_OFF(0);

		if (mod_mode_135==135){
			lock[2]=1-lock[2];
			if (lock[2]) LOCKLED_ON(2);
			else LOCKLED_OFF(2);

			lock[4]=1-lock[4];
			if (lock[4]) LOCKLED_ON(4);
			else LOCKLED_OFF(4);
		}
	}

	if (do_LOCK246){
		do_LOCK246=0;

		lock[5]=1-lock[5];
		if (lock[5]) LOCKLED_ON(5);
		else LOCKLED_OFF(5);


		if (mod_mode_246==246){
			lock[1]=1-lock[1];
			if (lock[1]) LOCKLED_ON(1);
			else LOCKLED_OFF(1);

			lock[3]=1-lock[3];
			if (lock[3]) LOCKLED_ON(3);
			else LOCKLED_OFF(3);
		}

	}
}


uint16_t rotsw_up, rotsw_down;
uint32_t rotary_button_hold_ctr;
uint8_t user_turned_rotary=0;

void process_rotary_button(void){
	static uint8_t just_switched_to_change_scale_mode=0;
	uint8_t i;


	if (ROTARY_SW){
		rotsw_up=1;

		if (rotsw_down!=0) rotsw_down++;

		//Handle long hold press:
		if (rotary_button_hold_ctr!=0) rotary_button_hold_ctr++; //when set to 0, we have disabled counting

		if (ui_mode==PLAY && rotary_button_hold_ctr>150000){
			rotary_button_hold_ctr=0;

			change_scale_mode=0;
			ui_mode=PRE_SELECT_PARAMS;
		}

		if (ui_mode==SELECT_PARAMS && rotary_button_hold_ctr>150000){
			rotary_button_hold_ctr=0;

			ui_mode=PRE_EDIT_COLORS;
		}

		if (ui_mode==EDIT_COLORS && rotary_button_hold_ctr>150000){
			rotary_button_hold_ctr=0;

			exit_system_mode(1); //restore lock[] so that we can save them
			save_param_bank(cur_param_bank);
		}

		if (ui_mode==EDIT_SCALES && rotary_button_hold_ctr>150000){
			rotary_button_hold_ctr=0;
			exit_edit_scale(1);
		}


		if (rotsw_down>5){rotsw_down=0;

			//Handle button press
			if ((ui_mode==PLAY || ui_mode==EDIT_SCALES) && change_scale_mode==0) {

				change_scale_mode=1;
				just_switched_to_change_scale_mode=1;

			} else if (change_scale_mode==1) {

				just_switched_to_change_scale_mode=0;
			}

			user_turned_rotary=0;

		}

	} else {
		rotsw_down=1;rotary_button_hold_ctr=1;
		if (rotsw_up!=0) rotsw_up++;
		if (rotsw_up>5){ rotsw_up=0;

			//Handle button release
			if (ui_mode==SELECT_PARAMS){
				exit_select_colors_mode();
				exit_system_mode(1); //restore lock[] and return to PLAY mode
			}

			if (ui_mode==PRE_SELECT_PARAMS){
				enter_system_mode();
			}

			if (ui_mode==EDIT_COLORS){
				exit_system_mode(1); //restore lock[] and return to PLAY mode
			}

			if (ui_mode==PRE_EDIT_COLORS){
				enter_edit_color_mode();
			}

			if (ui_mode==PLAY){
				for (i=0;i<NUM_CHANNELS;i++) {
					if (lock[i]!=1) {
						scale_bank[i]=hover_scale_bank; //Set all unlocked scale_banks to the same value
					}
				}
			}

			if (change_scale_mode && !just_switched_to_change_scale_mode && !user_turned_rotary) {
				change_scale_mode=0;
				just_switched_to_change_scale_mode=0;
			} else if (change_scale_mode && just_switched_to_change_scale_mode && user_turned_rotary) {
				change_scale_mode=0;
				just_switched_to_change_scale_mode=0;
			}
		}
	}


}

void process_rotary_rotation(void){
	rotary_state=read_rotary();

	if (rotary_state==DIR_CW) {

		if (ui_mode==SELECT_PARAMS){		 //COLOR SCHEME

			cur_colsch=(cur_colsch+1) % NUM_COLORSCHEMES;
		}

		else if (ui_mode==PLAY && rotsw_up && !rotsw_down){	//HOVER BANK
			//block ability to enter select colors mode if we turn the rotary
			rotary_button_hold_ctr=0;

			hover_scale_bank++;
			if (hover_scale_bank==NUMSCALEBANKS) hover_scale_bank=0; //wrap-around
			user_turned_rotary=1;
		}
		else if (!change_scale_mode){	//ROTATE UP

			if (ui_mode==EDIT_SCALES) editscale_notelocked=1;

			rotate_up();
		}
		else{							//CHANGE SCALE

			if (ui_mode==EDIT_SCALES) editscale_notelocked=1;

			change_scale_up();

		}
	}
	if (rotary_state==DIR_CCW) {
		if (ui_mode==SELECT_PARAMS){ //HOVER COLOR SCHEME

			if (cur_colsch==0) cur_colsch = NUM_COLORSCHEMES-1;
			else cur_colsch--;
		}

		else if (ui_mode==PLAY && rotsw_up && !rotsw_down){ //HOVER BANK
			//block ability to enter select colors mode if we turn the rotary
			rotary_button_hold_ctr=0;

			if (hover_scale_bank==0) hover_scale_bank=NUMSCALEBANKS-1; //wrap-around
			else hover_scale_bank--;

			user_turned_rotary=1;

		} else if (!change_scale_mode){ //ROTATE DOWN
			if (ui_mode==EDIT_SCALES) editscale_notelocked=1;

			rotate_down();

		}else {							//CHANGE SCALE
			if (ui_mode==EDIT_SCALES) editscale_notelocked=1;

			change_scale_down();
		}
	}

}

//Reads ADC, applies hysteresis correction and returns 1 if spread value has changed
int8_t read_spread(void){

	uint8_t test_spread=0, hys_spread=0;
	uint16_t temp_u16;

	// Hysteresis is used to ignore noisy ADC values.
	// We are checking if Spread ADC has changed values enough to warrant a change our "spread" variable.
	// Like a Schmitt trigger, we set a different threshold (temp_u16) depending on if the ADC value is rising or falling.

	test_spread=(adc_buffer[SPREAD_ADC] >> 8) + 1; //0-4095 to 1-16

	if (test_spread < spread){
		if (adc_buffer[SPREAD_ADC] <= (4095-SPREAD_ADC_HYSTERESIS))
			temp_u16 = adc_buffer[SPREAD_ADC] + SPREAD_ADC_HYSTERESIS;
		else
			temp_u16 = 4095;

		hys_spread = (temp_u16 >> 8) + 1;

	} else if (test_spread > spread){
		if (adc_buffer[SPREAD_ADC] > SPREAD_ADC_HYSTERESIS)
			temp_u16 = adc_buffer[SPREAD_ADC] - SPREAD_ADC_HYSTERESIS;
		else
			temp_u16 = 0;

		hys_spread = (temp_u16 >> 8) + 1;

	} else {
		hys_spread=0xFF; //adc has not changed, do nothing
	}

	if (hys_spread == test_spread) {
		return(hys_spread); //spread has changed
	}
	else return(-1); //spread has not changed



}


void process_rotateCV(void){
	static uint16_t old_rotcv_adc=0;
	static int8_t rot_offset=0;
	static int8_t old_rot_offset=0;
	int32_t t;

	t=(int16_t)adc_buffer[ROTCV_ADC] - (int16_t)old_rotcv_adc;
	if (t<(-100) || t>100){

		old_rotcv_adc=adc_buffer[ROTCV_ADC];

		rot_offset=adc_buffer[ROTCV_ADC]/205; //0..19

		jump_rotate_with_cv(rot_offset-old_rot_offset);

		old_rot_offset = rot_offset;
	}
}

void process_scaleCV(void){
	int32_t t;
	static uint16_t old_scalecv_adc=0;
	static int32_t t_scalecv=0;
	static int32_t t_old_scalecv=0;

	t=(int16_t)adc_buffer[SCALE_ADC] - (int16_t)old_scalecv_adc;
	if (t<(-100) || t>100){

		old_scalecv_adc = adc_buffer[SCALE_ADC];

		t_scalecv = adc_buffer[SCALE_ADC]/409; //0..10

		jump_scale_with_cv(t_scalecv - t_old_scalecv);

		t_old_scalecv = t_scalecv;
	}
}

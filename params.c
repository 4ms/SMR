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
#include "math.h"

extern __IO uint16_t adc_buffer[NUM_ADCS];
extern __IO uint16_t potadc_buffer[NUM_ADC3S];

enum Filter_Types filter_type=MAXQ;
enum Filter_Modes filter_mode=TWOPASS;

extern enum UI_Modes ui_mode;

extern uint8_t editscale_notelocked;
extern uint8_t editscale_tracklocked;

uint16_t old_adc_buffer[NUM_ADCS];
uint16_t old_potadc_buffer[NUM_ADC3S];

extern uint32_t rotary_state;

float trackcomp[NUM_CHANNELS]={1.0,1.0};
int16_t trackoffset[NUM_CHANNELS]={0,0};


//FREQ NUDGE/LOCK JACKS
float freq_nudge[NUM_CHANNELS]={1.0,1.0};
uint8_t ongoing_coarse_tuning[2]; 		// keeps track of ongoing coarse tunings 
uint8_t ongoing_fine_tuning[2]; 		// keeps track of ongoing fine tunings 
uint32_t fine_tuning_timeout[2]={0,0};	// timer to clear fine tuning when freq knob isn't being adjusted

float coarse_adj_led[NUM_CHANNELS];
float coarse_adj[NUM_CHANNELS]={1.0, 1.0, 1.0, 1.0, 1.0, 1.0};
float freq_shift[NUM_CHANNELS];
uint16_t mod_mode_135;
uint16_t mod_mode_246;

uint8_t fine_timer[2]={1,1}; 			// flag for fine tune display soft release 1: soft release. 0: immediate release

extern uint8_t do_LOCK135;
extern uint8_t do_LOCK246;
 
// ROTARY BUTTON
uint16_t rotsw_up, rotsw_down;
uint32_t rotary_button_hold_ctr;
uint8_t user_turned_rotary=0;
 
//LOCK BUTTONS
uint8_t lock[NUM_CHANNELS];  			// LATCH 0: channel unlocked, 1: channel locked
uint8_t lock_pressed[NUM_CHANNELS]; 	// MOMENTARY 1 while button pressed. 0 otherwise
uint8_t lock_up[NUM_CHANNELS];  		// MOMENTARY 1 while button pressed. 0 otherwise
uint32_t lock_down[NUM_CHANNELS];		// COUNTER starts at 1 and goes up while button's pressed
uint8_t num_clear_coarse_staged = 0; 	// number of coarse tunings staged to be cleared
uint8_t already_handled_lock_release[NUM_CHANNELS];

//ENV OUTS
uint32_t env_prepost_mode;
// BINARY CONVENTION USED
// left-most bit is the sign of the coarse adjustment (+/-  X semitone, 1 is going down semitones) rest is LED ON=1 OFF=0 is display order
int saved_envled_state[NUM_CHANNELS]={0b0001100,0b0001100,0b0001100,0b0001100,0b0001100,0b0001100};
int cur_envled_state=0b0000000;		// envled state for coarse tuning
int fine_envled=0b000000;			// envled state for fine tuning
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
	filter_mode=TWOPASS;

	trackcomp[0]=1.0;
	trackcomp[1]=1.0;
	trackoffset[0]=0;
	trackoffset[1]=0;
}

void param_read_freq(void){
	uint8_t i,j,k;
	float t_fo, t_fe;														// freq nudge knob readouts 
	static uint8_t sleep_range_saved =0, first_run[2]={1,1};
	static uint8_t old_switch_state[2];										// previous 135/246 switch state
	static uint8_t fknob_lock[2]={0,0};										// true disables nudge knob for odds/evens
	static uint8_t coarse_lock[NUM_CHANNELS]={1,1,1,1,1,1}; 				// same as fknoblock, for the coarse tuning
	static float sleep_range=800; 											// number of counts that won't wake up nudge knob after module is turned on
	static float wakeup_evens[2], wakeup_odds[2]; 							// value at which f_nudge knob wakes up after module turns on
	float f_nudge_odds=1, f_nudge_evens=1;
	static float old_f_nudge_odds=1, old_f_nudge_evens=1; 					// keeps track of freq knob rotation
	static float f_nudge_odds_buf, f_nudge_evens_buf;
	static float f_nudge_buf[2];
	static float f_shift_odds=1, f_shift_evens=1;
	static float saved_coarse_adj[NUM_CHANNELS]={1.0,1.0,1.0,1.0,1.0,1.0};	// value to cross in order to unlock coarse tuning
	static uint8_t clear_coarse_staged[NUM_CHANNELS]={0,0,0,0,0,0};
	int odds[3] ={0, 2, 4}; // ch 1, 3, 5
	int evens[3]={1, 3, 5}; // ch 2, 4, 6
	int32_t freq_jack_cv;
	
	// FREQ SHIFT
		//With the Maxq filter, the Freq Nudge pot alone adjusts the "nudge", and the CV jack is 1V/oct shift
		//With the BpRe filter, the Freq Nudge pot plus CV jack adjusts the "nudge", and there is no 1V/oct shift
		if (filter_type==MAXQ){
			// Read buffer knob and normalize input: 0-1
				t_fo=(float)(adc_buffer[FREQNUDGE1_ADC]);
				t_fe=(float)(adc_buffer[FREQNUDGE6_ADC]);
			// after turning on the module
			// keep freq nudge knobs output at 0 as long as knobs are within sleep range
				if (!sleep_range_saved){
					wakeup_odds[0] =t_fo - sleep_range;
					wakeup_odds[1] =t_fo + sleep_range;
					wakeup_evens[0]=t_fe - sleep_range;
					wakeup_evens[1]=t_fe + sleep_range;
					sleep_range_saved = 1; // ensures this only gets computed once
				}
				if (first_run[0]){
					if ((t_fo > wakeup_odds[0]) && (t_fo < wakeup_odds[1])) {t_fo=0.0;}
					else{first_run[0]=0;}
				}
				if (first_run[1]){
					if ((t_fe > wakeup_evens[0]) && (t_fe < wakeup_evens[1])) {t_fe=0.0;}
					else{first_run[1]=0;}
				}

			// Freq shift odds
			// is odds cv input Low-passed and adjusted for 1V/Oct
				if (trackcomp[0]<0.5 || trackcomp[0]>2.0) trackcomp[0]=1.0; //sanity check

				freq_jack_cv = (adc_buffer[FREQCV1_ADC] + trackoffset[0]) * trackcomp[0];
				if (freq_jack_cv<0) freq_jack_cv=0;
				if (freq_jack_cv>4095) freq_jack_cv=4095;

				f_shift_odds *= FREQCV_LPF; 
				f_shift_odds += (1.0f-FREQCV_LPF)*(float)(exp_1voct[freq_jack_cv]) ;

			// Freq shift evens
			// is odds cv input Low-passed and adjusted for 1V/Oct
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
	
	// FREQ NUDGE 
	  // SEMITONE FINE TUNE	
		f_nudge_odds  = 1 + t_fo/55000.0; // goes beyond semitone 
		f_nudge_evens = 1 + t_fe/55000.0; 

	  // 12-SEMITONE COARSE TUNE
		// [freq nudge knob] + [lock button] -> coarse tune
		
		// ODDS
		if (fabsf(f_nudge_odds - old_f_nudge_odds) > NUDGEPOT_MIN_CHANGE){ 
						
			// inform led_ring_c that fine tuning is ongoing
			ongoing_fine_tuning[0]=1;
			
			// update buffers
			f_nudge_odds_buf = old_f_nudge_odds;
			old_f_nudge_odds=f_nudge_odds;

			// enable fine tuning timer for slow display release
			fine_timer[0] = 1;	
			
			// for each odd channel			
			for (i=0;i<3;i++){
				j = odds[i];
				
				//update fine tuning env led
				fine_envled = fine_envled | (1<<(5-j));

				// inform led_ring_c that fine tuning is ongoing on odd chan(s)
				ongoing_fine_tuning[0]	= 1;
				fine_tuning_timeout[0] 	= 1; 	// stage timeout counter
					 
				// If lock button pressed, disable fine tuning and process coarse tuning				
				if (lock_pressed[j]){ 
					
					// inform led_ring_c that coarse tuning is ongoing
					ongoing_coarse_tuning[0]=1;
					
					// disable fine tuning timer for instantaneous display release
					fine_timer[0] = 0;

					// coarse tuning superseds fine tuning
					ongoing_fine_tuning[0] = 0;
										
					// disable fine adjustments when setting coarse tuning
					if (fknob_lock[0]==0){ 
						f_nudge_buf[0] = f_nudge_odds_buf;
						fknob_lock[0]  = 1;
					}	
				  // Compute and apply coarse tuning	 		
			 		// - 6 semitones
			 		if (t_fo < 1.0*4095.0/13.0){coarse_adj[j]=1.0/1.41421356237;cur_envled_state=0b1111111;}
			 		else if ((t_fo >= 1.0*4095.0/13.0) && (t_fo < 2.0*4095.0/13.0)){coarse_adj[j]=1.0/1.33483985417;cur_envled_state=0b1011111;}
			 		else if ((t_fo >= 2.0*4095.0/13.0) && (t_fo < 3.0*4095.0/13.0)){coarse_adj[j]=1.0/1.25992104989;cur_envled_state=0b1001111;}
			 		else if ((t_fo >= 3.0*4095.0/13.0) && (t_fo < 4.0*4095.0/13.0)){coarse_adj[j]=1.0/1.189207115;  cur_envled_state=0b1000111;}
			 		else if ((t_fo >= 4.0*4095.0/13.0) && (t_fo < 5.0*4095.0/13.0)){coarse_adj[j]=1.0/1.12246204831;cur_envled_state=0b1000011;}
			 		else if ((t_fo >= 5.0*4095.0/13.0) && (t_fo < 6.0*4095.0/13.0)){coarse_adj[j]=1.0/1.05946309436;cur_envled_state=0b1000001;}

					//Tuned
			 		else if ((t_fo >= 6.0*4095.0/13.0) && (t_fo < 7.0*4095.0/13.0)){coarse_adj[j]=1.0;cur_envled_state=0b0001100;}

			 		// + 6 semitones
			 		else if ((t_fo >= 7.0*4095.0/13.0) && (t_fo < 8.0*4095.0/13.0)){coarse_adj[j]=1.05946309436;	cur_envled_state=0b0100000;}
			 		else if ((t_fo >= 8.0*4095.0/13.0) && (t_fo < 9.0*4095.0/13.0)){coarse_adj[j]=1.12246204831;	cur_envled_state=0b0110000;}
			 		else if ((t_fo >= 9.0*4095.0/13.0) && (t_fo < 10.0*4095.0/13.0)){coarse_adj[j]=1.189207115; 	cur_envled_state=0b0111000;}
			 		else if ((t_fo >= 10.0*4095.0/13.0) && (t_fo < 11.0*4095.0/13.0)){coarse_adj[j]=1.25992104989; 	cur_envled_state=0b0111100;}
			 		else if ((t_fo >= 11.0*4095.0/13.0) && (t_fo < 12.0*4095.0/13.0)){coarse_adj[j]=1.33483985417;	cur_envled_state=0b0111110;}
			 		else if ((t_fo >= 12.0*4095.0/13.0) && (t_fo < 13.0*4095.0/13.0)){coarse_adj[j]=1.41421356237;	cur_envled_state=0b0111111;}
					
			 		// apply/clear coarse lock and corresponding envled_state as needed
			 		if ( fabsf(coarse_adj[j] - saved_coarse_adj[j])< 0.001){coarse_lock[j]=0;}	
			 		if(coarse_lock[j]){coarse_adj[j] = saved_coarse_adj[j];cur_envled_state=saved_envled_state[j];}
			 		else{saved_coarse_adj[j]=coarse_adj[j];saved_envled_state[j]=cur_envled_state;}
			 		
			 		// apply coarse adjustment	
			 		freq_nudge[j] = f_nudge_buf[1] * coarse_adj[j];
					already_handled_lock_release[j] = 1; //set this flag so that we don't do anything when the button is released
		 		}else{coarse_lock[j]=1;}
		 	}
		}
		
		// EVENS
		if (fabsf(f_nudge_evens - old_f_nudge_evens) > NUDGEPOT_MIN_CHANGE){ 
			
			// inform led_ring_c that fine tuning is ongoing
			ongoing_fine_tuning[1]=1;
			
			// update buffers
			f_nudge_evens_buf = old_f_nudge_evens;
			old_f_nudge_evens=f_nudge_evens;
			
			// enable fine tuning timer for slow display release
			fine_timer[1]	= 1;	
			
			// for each even channel
			for (i=0;i<3;i++){
				j = evens[i];	
		
				//update fine tuning env led
				fine_envled = fine_envled | (1<<(5-j));

				// inform led_ring_c that fine tuning is ongoing on even chan(s)
				ongoing_fine_tuning[1] 	= 1;
				fine_tuning_timeout[1] 	= 1; 	// stage timeout counter
				
				// If lock button pressed, disable fine tuning and process coarse tuning							
				if (lock_pressed[j]){
			
					// inform led_ring_c that coarse tuning is ongoing
					ongoing_coarse_tuning[1]=1;
				
					// coarse tuning superseds fine tuning
					ongoing_fine_tuning[1] = 0;
					
					// disable fine tuning timer for instantaneous display release
					fine_timer[1] = 0;
					
					// disable fine adjustments when setting coarse tuning
					if (fknob_lock[1]==0){ 
						f_nudge_buf[1] = f_nudge_evens_buf;
						fknob_lock[1]  = 1;
					}	
				  // Compute and apply coarse tuning	 		
			 		// - 6 semitones
			 		if (t_fe < 1.0*4095.0/13.0){coarse_adj[j]=1.0/1.41421356237;cur_envled_state=0b1111111;}
			 		else if ((t_fe >= 1.0*4095.0/13.0) && (t_fe < 2.0*4095.0/13.0)){coarse_adj[j]=1.0/1.33483985417;cur_envled_state=0b1011111;}
			 		else if ((t_fe >= 2.0*4095.0/13.0) && (t_fe < 3.0*4095.0/13.0)){coarse_adj[j]=1.0/1.25992104989;cur_envled_state=0b1001111;}
			 		else if ((t_fe >= 3.0*4095.0/13.0) && (t_fe < 4.0*4095.0/13.0)){coarse_adj[j]=1.0/1.189207115;  cur_envled_state=0b1000111;}
			 		else if ((t_fe >= 4.0*4095.0/13.0) && (t_fe < 5.0*4095.0/13.0)){coarse_adj[j]=1.0/1.12246204831;cur_envled_state=0b1000011;}
			 		else if ((t_fe >= 5.0*4095.0/13.0) && (t_fe < 6.0*4095.0/13.0)){coarse_adj[j]=1.0/1.05946309436;cur_envled_state=0b1000001;}

					//Tuned
			 		else if ((t_fe >= 6.0*4095.0/13.0) && (t_fe < 7.0*4095.0/13.0)){coarse_adj[j]=1.0;cur_envled_state=0b0001100;}

			 		// + 6 semitones
			 		else if ((t_fe >= 7.0*4095.0/13.0) && (t_fe < 8.0*4095.0/13.0)){coarse_adj[j]=1.05946309436;	cur_envled_state=0b0100000;}
			 		else if ((t_fe >= 8.0*4095.0/13.0) && (t_fe < 9.0*4095.0/13.0)){coarse_adj[j]=1.12246204831;	cur_envled_state=0b0110000;}
			 		else if ((t_fe >= 9.0*4095.0/13.0) && (t_fe < 10.0*4095.0/13.0)){coarse_adj[j]=1.189207115; 	cur_envled_state=0b0111000;}
			 		else if ((t_fe >= 10.0*4095.0/13.0) && (t_fe < 11.0*4095.0/13.0)){coarse_adj[j]=1.25992104989; 	cur_envled_state=0b0111100;}
			 		else if ((t_fe >= 11.0*4095.0/13.0) && (t_fe < 12.0*4095.0/13.0)){coarse_adj[j]=1.33483985417;	cur_envled_state=0b0111110;}
			 		else if ((t_fe >= 12.0*4095.0/13.0) && (t_fe < 13.0*4095.0/13.0)){coarse_adj[j]=1.41421356237;	cur_envled_state=0b0111111;}

			 		// apply/clear coarse lock and corresponding envled_state as needed
			 		if ( fabsf(coarse_adj[j] - saved_coarse_adj[j])< 0.001){coarse_lock[j]=0;}	
			 		if(coarse_lock[j]){coarse_adj[j] = saved_coarse_adj[j];cur_envled_state=saved_envled_state[j];}
			 		else{saved_coarse_adj[j]=coarse_adj[j];saved_envled_state[j]=cur_envled_state;}
			 		
			 		// apply coarse adjustment	
			 		freq_nudge[j] = f_nudge_buf[1] * coarse_adj[j];
					already_handled_lock_release[j] = 1; 
		 		}else{coarse_lock[j]=1;}
		 	}
		}
	 	
	 // MORE FINE/COARSE TUNE DISPLAY CASES
	 	// stop displaying fine tune if fine tune knob is not being adjusted but leave display on for timed duration 	
	  	// special case: both fine tunes are ajusted at once. Leave more time to facilitate tuning odds/even together	
	 	else if ((ongoing_fine_tuning[0]==1) && (ongoing_fine_tuning[1]==1)){ 
			fine_tuning_timeout[0]+=1;
			fine_tuning_timeout[1]+=1;
			if((fine_tuning_timeout[0]> (DISPLAYTIME_NUDGEBOTHSIDES * fine_timer[0])) && (fine_tuning_timeout[1]> (DISPLAYTIME_NUDGEBOTHSIDES * fine_timer[1]))  ){
				ongoing_fine_tuning[0]=0;
				ongoing_fine_tuning[1]=0;
				fine_envled = fine_envled & 0b000000; // turn off all env led
				fine_tuning_timeout[0]=0;
				fine_tuning_timeout[1]=0;
			}
		// turn off odds fine tune display after short time
		} else if ((ongoing_fine_tuning[0]==1) && (ongoing_fine_tuning[1]==0)){ 
			fine_tuning_timeout[0]+=1;
			if(fine_tuning_timeout[0]> (DISPLAYTIME_NUDGEONESIDE * fine_timer[0])){
				ongoing_fine_tuning[0]=0;
				fine_envled = fine_envled & 0b010101; // turn off odds env led
				fine_tuning_timeout[0]=0;
			}
		// turn off evens fine tune display after short time 	
		} else if ((ongoing_fine_tuning[1]==1) && (ongoing_fine_tuning[0]==0)){ 
			fine_tuning_timeout[1]+=1;
			if(fine_tuning_timeout[1]> (DISPLAYTIME_NUDGEONESIDE * fine_timer[1])){
				ongoing_fine_tuning[1]=0;
				fine_envled = fine_envled & 0b101010; // turn off even env led
				fine_tuning_timeout[1]=0;
			}
		}

	  
		// display fine tune when locked button is held down
		if (lock_pressed[0] || lock_pressed[2] || lock_pressed[4]) {
			fine_envled=fine_envled | 0b101010; 
			ongoing_fine_tuning[0]=1;
			fine_tuning_timeout[0]=1;
			if (!ongoing_coarse_tuning[0]) fine_timer[0] = 1; //enable soft release
		} 
		if (lock_pressed[1] || lock_pressed[3] || lock_pressed[5]) {
			fine_envled=fine_envled | 0b010101; 
			ongoing_fine_tuning[1]=1;
			fine_tuning_timeout[1]=1;
			if (!ongoing_coarse_tuning[1]) fine_timer[1] = 1; //enable soft release
		}
		
		// display fine tune when lock switch state is changed
		// switch 135
		if (mod_mode_135!=old_switch_state[0]){
			old_switch_state[0] = mod_mode_135;
			fine_envled=fine_envled | 0b101010; 
			ongoing_fine_tuning[0]=1;
			fine_tuning_timeout[0]=1; //reset the timer so that we get the full soft-release period
			fine_timer[0] = 1; //enable soft release
		}
		// switch 246
		if (mod_mode_246!=old_switch_state[1]){
			old_switch_state[1] = mod_mode_246;
			fine_envled=fine_envled | 0b010101; 
			ongoing_fine_tuning[1]=1;
			fine_tuning_timeout[1]=1; //reset the timer so that we get the full soft-release period
			fine_timer[1] = 1; //enable soft release
		}

	  	// exit coarse tuning display as needed
		if(ongoing_coarse_tuning[0]){
			if (!lock_pressed[0] && !lock_pressed[2] && !lock_pressed[4]){
				ongoing_coarse_tuning[0]=0;
			}
		}
		if(ongoing_coarse_tuning[1]){
			if (!lock_pressed[1] && !lock_pressed[3] && !lock_pressed[5]){
				ongoing_coarse_tuning[1]=0;
			}
		}		
		
	// LOCK SWITCHES
	// nudge and shift always enabled on 1 and 6
	// ... and enabled on 3,5,2 and 4 based on the lock toggles
	  // ODDS
		// enable freq nudge and shift for "135 mode"
		if (!lock[0]){
		  	// Prevent fknob from adjusting freq_nudge when it's fknob_lock(ed)
			if (fknob_lock[0]==1){ 																			// if freq knob is locked
				// during coarse adj
				if((fabsf(f_nudge_odds - f_nudge_buf[0]) < 0.001)  && !lock_pressed[0]){fknob_lock[0]=0;} 	// unlock odds/evens freq knob if it crosses the value it was locked on and lock button isn't pressed 	
				else {freq_nudge[0]=f_nudge_buf[0] * coarse_adj[0];} 										// use buffered value otherwise
			}
			if (fknob_lock[0]==0){freq_nudge[0]=f_nudge_odds * coarse_adj[0];};		 						// if freq knob is unlocked, apply coarse adjustment to current f_knob fine tune
		}
		freq_shift[0]=f_shift_odds; // apply freq CV in
		
		if (mod_mode_135==135){
			if (!lock[2]){
				if (fknob_lock[0]==1){ 								
					if((fabsf(f_nudge_odds - f_nudge_buf[0]) < 0.001) && !lock_pressed[2]){fknob_lock[0]=0;}
					else {freq_nudge[2]=f_nudge_buf[0] * coarse_adj[2];}
				}
				if (fknob_lock[0]==0){freq_nudge[2]=f_nudge_odds * coarse_adj[2];}; 
			}
			freq_shift[2]=f_shift_odds;

			if (!lock[4]){
				if (fknob_lock[0]==1){ 								
					if((fabsf(f_nudge_odds - f_nudge_buf[0]) < 0.001) && !lock_pressed[4]){fknob_lock[0]=0;}
					else {freq_nudge[4]=f_nudge_buf[0] * coarse_adj[4];}
				}
				if (fknob_lock[0]==0){freq_nudge[4]=f_nudge_odds * coarse_adj[4];}; 
			}
			freq_shift[4]=f_shift_odds;
		} 
		// disable freq nudge and shift on channel 3 and 5 when in "1 mode"
		else { 
			if (!lock[2]){
				freq_nudge[2]= coarse_adj[2];
			}
			freq_shift[2]=1.0;

			if (!lock[4]){
				freq_nudge[4]= coarse_adj[4];
			}
			freq_shift[4]=1.0;
		}
		
	  //EVENS
		if (!lock[5]){
			if (fknob_lock[1]==1){ 													
				if((fabsf(f_nudge_evens - f_nudge_buf[1]) < 0.001) && !lock_pressed[5]){fknob_lock[1]=0;} 	
				else {freq_nudge[5]=f_nudge_buf[1] * coarse_adj[5];} 			
			}
			if (fknob_lock[1]==0){freq_nudge[5]=f_nudge_evens * coarse_adj[5];}; 
		}
		freq_shift[5]=f_shift_evens; 
		
		if (mod_mode_246==246){
			if (!lock[1]){
				if (fknob_lock[1]==1){ 								
					if((fabsf(f_nudge_evens - f_nudge_buf[1]) < 0.001) && !lock_pressed[1]){fknob_lock[1]=0;}
					else {freq_nudge[1]=f_nudge_buf[1] * coarse_adj[1];}
				}
				if (fknob_lock[1]==0){freq_nudge[1]=f_nudge_evens * coarse_adj[1];}; 
			}
			freq_shift[1]=f_shift_evens;

			if (!lock[3]){
				if (fknob_lock[1]==1){ 								
					if((fabsf(f_nudge_evens - f_nudge_buf[1]) < 0.001) && !lock_pressed[3]){fknob_lock[1]=0;}
					else {freq_nudge[3]=f_nudge_buf[1] * coarse_adj[3];}
				}
				if (fknob_lock[1]==0){freq_nudge[3]=f_nudge_evens * coarse_adj[3];}; 
			}
			freq_shift[3]=f_shift_evens;
		} 
		// disable freq nudge and shift on channel 2 and 4 when in "6 mode"
		else {
			if (!lock[3]){
				freq_nudge[3]= coarse_adj[3];
			}
			freq_shift[3]=1.0;

			if (!lock[1]){
				freq_nudge[1]= coarse_adj[1];
			}
			freq_shift[1]=1.0;
		}
		
	// CLEAR COARSE TUNING AT BUTTON RELEASE AFTER 6 BUTTON PRESS (~3s)
		// if locked buttons have all been pressed for +3s
		if (lock_down[0] > 36000 && 
 			lock_down[1] > 36000 &&  
 			lock_down[2] > 36000 && 
 			lock_down[3] > 36000 && 
 			lock_down[4] > 36000 && 
 			lock_down[5] > 36000 ){
		 	clear_coarse_staged[0] = 1; 			
			clear_coarse_staged[1] = 1;  
			clear_coarse_staged[2] = 1; 
			clear_coarse_staged[3] = 1; 
			clear_coarse_staged[4] = 1; 
			clear_coarse_staged[5] = 1;
			num_clear_coarse_staged=6;
		} else{
			coarse_adj_led[0]=coarse_adj[0];
			coarse_adj_led[1]=coarse_adj[1];
			coarse_adj_led[2]=coarse_adj[2];
			coarse_adj_led[3]=coarse_adj[3];
			coarse_adj_led[4]=coarse_adj[4];
			coarse_adj_led[5]=coarse_adj[5];
		}

		if (num_clear_coarse_staged !=0){												
			for (k=0;k<6;k++){
				if (clear_coarse_staged[k]){
					// do not change lock states upon button release
					already_handled_lock_release[k]=1;
					if (!lock_pressed[k]){
						// clear coarse adjustment
						coarse_adj[k]=1.0;

						//update saved coarse adjustement + env led state & lock to it
						saved_coarse_adj[k]=coarse_adj[k];
						saved_envled_state[k]=0b0001100;
						coarse_lock[k]=1;

						// inform led.c that this coarse tuning was cleared
						coarse_adj_led[k] = coarse_adj[k];						
						
						// unstage coarse adjustment
						clear_coarse_staged[k]=0;
						// remove channel from list of adjustments to be cleared
						num_clear_coarse_staged-=1;	
						//
						already_handled_lock_release[k]=0;	
					}
				} 
			}	
		}

}

void param_read_channel_level(void){
	float level_lpf;
	uint8_t i;
	uint16_t t;

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

void param_read_one_channel_level(uint8_t i)
{
	uint16_t t;
	float level_lpf;

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

		// LPF qpot
		t = potadc_buffer[QPOT_ADC];
 	 	qpot_lpf *= QPOT_LPF;
 	 	qpot_lpf += (1.0f-QPOT_LPF)*t;
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
					}else if (!lock[i]){qvalpot=qpot_lpf;}
				}
			}
		}
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
		env_prepost_mode=POST;
	} else {
		env_prepost_mode=PRE;
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
		env_track_mode=ENV_VOLTOCT;
		//env_track_mode=ENV_FAST;
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

		if (lag_val){ //CVLAG switch is flipped on, latch the current Morph adc value and use that to calculate LPF coefficients
			lag_val=adc_buffer[MORPH_ADC];
			if (lag_val<200) lag_val=200; //force some amount of CV Slew even if Morph knob is all the way down

			t_LEVEL_LPF_ATTACK=	1.0 - (1.0/((lag_val)*0.10)); //0.95 to 0.997558
			t_LEVEL_LPF_DECAY=	1.0 - (1.0/((lag_val)*0.25)); //0.98 to 0.999023

			if (t_LEVEL_LPF_ATTACK<0 || t_LEVEL_LPF_ATTACK>=1)	LEVEL_LPF_ATTACK=LAG_ATTACK_MIN_LPF;
			else LEVEL_LPF_ATTACK = t_LEVEL_LPF_ATTACK;

			if (t_LEVEL_LPF_DECAY<0 || t_LEVEL_LPF_DECAY>=1)	LEVEL_LPF_DECAY=LAG_DECAY_MIN_LPF;
			else LEVEL_LPF_DECAY = t_LEVEL_LPF_DECAY;



		}else{ //CVLAG switch is flipped off
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



void process_lock_buttons(){
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
				if (lock_down[i]>LOCK_BUTTON_QUNLOCK_HOLD_TIME && lock[i] && !user_turned_Q_pot && q_locked[i]) {
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

void init_freq_update_timer(void)
{
	TIM_TimeBaseInitTypeDef  tim;

	NVIC_InitTypeDef nvic;
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_TIM10, ENABLE);

	nvic.NVIC_IRQChannel = TIM1_UP_TIM10_IRQn;
	nvic.NVIC_IRQChannelPreemptionPriority = 3;
	nvic.NVIC_IRQChannelSubPriority = 3;
	nvic.NVIC_IRQChannelCmd = ENABLE;
	NVIC_Init(&nvic);

	//Triangle wave into the Freq jack. Measure the amplitude of the triangle wave coming out of the VOCT jack, expressed in % of the input amplitude.

	//TIM_Period of 32767 = ~5kHz =====> 19Hz outputs ~50% amplitude
	//TIM_Period of 16383 = ~10kHz =====> 37Hz outputs ~50% amplitude
	//TIM_Period of 8195 = ~20kHz =====> 55Hz outputs ~50% amplitude (less than expected, perhaps due to the hardware LPF on the Freq jack input? or the software LPF?)

	//168MHz / (Period+1)
	TIM_TimeBaseStructInit(&tim);
	tim.TIM_Period = 8195;
	tim.TIM_Prescaler = 0;
	tim.TIM_ClockDivision = 0;
	tim.TIM_CounterMode = TIM_CounterMode_Up;

	TIM_TimeBaseInit(TIM10, &tim);

	TIM_ITConfig(TIM10, TIM_IT_Update, ENABLE);

	TIM_Cmd(TIM10, ENABLE);
}

void TIM1_UP_TIM10_IRQHandler(void)
{
	if (TIM_GetITStatus(TIM10, TIM_IT_Update) != RESET) {

		param_read_freq();

		TIM_ClearITPendingBit(TIM10, TIM_IT_Update);

	}
}


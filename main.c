/*
 * To Do
 *
 * Pressing the lcok button should lock the channel where it exists at that moment, not at the dest_filter
 * --we'll have to check if that spot conflicts with any other channel's dest_filter, and rotate them up/down if so
 *
 * Change scale should morph(?)... changing bank??
 *
 * Try single-variable Resonant BP (chrissy's link)
 * --freq_nudge could then be full range
 *
 * Figure out why low Q values are peaking
 *
 * Set ENVSPEED lpf values (it clips easily)
 *
 *
 * Hardware:
 * Add jumper to select sliders showing clip/cv level
 * (or have a system mode?)
 *
 * Add a jumper to allow DC signals (just jump the 10uF caps)
 *
 * What is that background sound?
 *
 * Add analog white noise circuit
 *
 */
#include "stm32f4xx.h"

#include "codec.h"
#include "i2s.h"
#include "adc.h"
#include "audio_util.h"
#include "memory.h"
#include "inouts.h"
#include "globals.h"
#include "filter.h"
#include "envout_pwm.h"
#include "led_driver.h"
#include "dac.h"
#include "rotary.h"
#include "rotation.h"

//#define TEST_LED_RING

//10s  is 0.999998958333873 or 1-0.00000104166613  @4096
//1s   is 0.999989583387583 or 1-0.00001041661242  @2048
//0.1s is 0.999895838758489 or 1-0.00010416124151  @1024
//10ms is 0.99895838758489  or 1-0.0010416124151  @512

#define LAG_ATTACK 0.9895838758489;
#define LAG_DECAY 0.99895838758489;

#define MAX_LAG_ATTACK 0.005
#define MIN_LAG_ATTACK 0.05

#define MAX_LAG_DECAY 0.001
#define MIN_LAG_DECAY 0.01

#define QPOT_MIN_CHANGE 50
#define LOCK_BUTTON_QUNLOCK_HOLD_TIME 50000

extern uint8_t scale_bank_defaultscale[NUMSCALEBANKS];

uint32_t g_error=0;

__IO uint16_t adc_buffer[NUM_ADCS];
__IO uint16_t potadc_buffer[NUM_ADC3S];

uint8_t rotate=0;
uint8_t note[NUM_CHANNELS];
uint8_t scale[NUM_CHANNELS];
uint8_t scale_cv[NUM_CHANNELS];
uint8_t scale_bank[NUM_CHANNELS];

int8_t motion_rotate[NUM_CHANNELS];
int8_t motion_spread[NUM_CHANNELS];
int8_t motion_notejump[NUM_CHANNELS];
int8_t motion_scale[NUM_CHANNELS];
uint8_t motion_dst_note[NUM_CHANNELS];
uint8_t motion_dst_scale[NUM_CHANNELS];
uint8_t motion_dst_bank[NUM_CHANNELS];

uint8_t hover_scale_bank=0;

const uint32_t slider_led[6]={LED_SLIDER1, LED_SLIDER2, LED_SLIDER3, LED_SLIDER4, LED_SLIDER5, LED_SLIDER6};

uint8_t lock[NUM_CHANNELS];
uint8_t lock_pressed[NUM_CHANNELS];
uint8_t lock_up[NUM_CHANNELS];
uint32_t lock_down[NUM_CHANNELS];

uint8_t flag_update_LED_ring=0;

float spectral_readout[NUM_FILTS];

extern uint8_t do_ROTUP;
extern uint8_t do_ROTDOWN;
extern uint8_t do_LOCK135;
extern uint8_t do_LOCK246;

extern float rot_dir[6];

extern uint16_t mod_mode_135, mod_mode_246;
extern uint16_t rotate_to_next_scale;

extern int8_t spread;

uint32_t qval[NUM_CHANNELS];
uint32_t qvalcv, qvalpot;
uint8_t q_locked[NUM_CHANNELS]={0,0,0,0,0,0};



float freq_nudge[NUM_CHANNELS];

extern uint32_t ENVOUT_PWM[NUM_CHANNELS];

uint8_t SLIDER_LEDS=SHOW_LEVEL;


#define delay()						\
do {							\
  register unsigned int i;				\
  for (i = 0; i < 1000000; ++i)				\
    __asm__ __volatile__ ("nop\n\t":::"memory");	\
} while (0)

void check_errors(void){

	//if (g_error)
		//LED_ON(LED_BLUE);

}

inline uint32_t diff(uint32_t a, uint32_t b){
	return (a>b)?(a-b):(b-a);
}

extern float f_adc[NUM_CHANNELS];
extern float motion_morphpos[NUM_CHANNELS];



/* ledring.c */
//#define ASSIGN_COLORS
#define NUM_COLORSCHEMES 7

uint8_t cur_colsch=0;

float COLOR_CH[NUM_COLORSCHEMES][NUM_CHANNELS][3]={
		{{0, 0, 761}, {0, 770, 766}, {0, 766, 16}, {389, 383, 387}, {763, 154, 0}, {766, 0, 112}},
		{{0, 0, 761}, {0, 780, 766}, {768, 767, 764}, {493, 768, 2}, {763, 154, 0}, {580, 65, 112}},
		{{767, 0, 0}, {767, 28, 386}, {0, 0, 764}, {0, 320, 387}, {767, 768, 1}, {767, 774, 765}},
		{{0, 0, 761}, {106, 0, 508}, {762, 769, 764}, {0, 767, 1}, {706, 697, 1}, {765, 179, 1}},
		{{0,0,900}, {200,200,816}, {800,900,800},	{900,400,800},	{900,500,202}, {800,200,0}},
		{{0,0,766}, {766,150,0}, {0,50,766}, {766,100,0}, {0,150,766}, {766,50,0}},
		{{0,0,1000}, {0,100,766}, {0,200,666}, {0,300,500}, {0,350,500}, {0,350,400}}
};

const float SCALE_BANK_COLOR[NUMSCALEBANKS][3]={

		{1000,1000,1000}, 	//white: western
		{1000,0,0},		//red: indian
		{0,0,1000},		//blue: alpha sp2
		{0,800,800},		//cyan: alpha sp1
		{0,1000,0},		//green: gamma sp1
		{1000,1000,0},		//yellow: 17ET
		{800,0,800},		//pink: twelve tone
		{700,0,100}		//orange: diatonic1

};


void display_filter_rotation(void){
	uint16_t ring[20][3];
	uint16_t env_out_leds[6][3];
	uint8_t i, next_i,chan=0;
	float inv_fade[6],fade[6];
	float t_f;
	static uint8_t flash=0;

	uint16_t ring_a[20][3];
	uint16_t ring_b[20][3];

	//12us to 100us
DEBUG3_ON;

#ifdef TEST_LED_RING
	for (chan=0;chan<20;chan++){
		ring[chan][0]=512;
		ring[chan][1]=512;
		ring[chan][2]=512;
	}

	for (chan=0;chan<6;chan++){
		env_out_leds[chan][0]=512;
		env_out_leds[chan][1]=512;
		env_out_leds[chan][2]=512;
	}


#else

	for (i=0;i<20;i++){
		ring[i][0]=0;
		ring[i][1]=0;
		ring[i][2]=0;
	}

	// Set the brightness of each LED in the ring:
	// --if it's unlocked, then brightness corresponds to the slider+cv level. Keep a minimum of 5% so that it doesn't go totally off
	// --if it's locked, brightness flashes between 100% and 0%.
	// As we rotate morph between two LEDs in the ring:
	// --fade[chan] is the brightness of the end point LED
	// --inv_fade[chan] is the brightness of the start point LED
	flash=1-flash;
	for (chan=0;chan<6;chan++){
		t_f = f_adc[chan] < 0.05 ? 0.05 : f_adc[chan];

		if (lock[chan]==0){
			inv_fade[chan] = (1.0-motion_morphpos[chan])*(t_f);
			fade[chan] = motion_morphpos[chan]*(t_f);

		} else {
			fade[chan]=0.0;
			if (flash) inv_fade[chan]=t_f;
			else inv_fade[chan]=0.0;
		}
	}

	for (i=0;i<20;i++){
	//	next_i=(i+1) % NUM_FILTS;
		for (chan=0;chan<6;chan++){
			next_i=motion_dst_note[chan];
			if (note[chan]==i){

				if (inv_fade[chan]>0.0){
					if (ring[i][0]+ring[i][1]+ring[i][2]==0){
						ring[i][0] = (uint16_t)((COLOR_CH[cur_colsch][chan][0])*inv_fade[chan]);
						ring[i][1] = (uint16_t)((COLOR_CH[cur_colsch][chan][1])*inv_fade[chan]);
						ring[i][2] = (uint16_t)((COLOR_CH[cur_colsch][chan][2])*inv_fade[chan]);
					}else {
						ring[i][0] += (uint16_t)((COLOR_CH[cur_colsch][chan][0])*inv_fade[chan]);
						ring[i][1] += (uint16_t)((COLOR_CH[cur_colsch][chan][1])*inv_fade[chan]);
						ring[i][2] += (uint16_t)((COLOR_CH[cur_colsch][chan][2])*inv_fade[chan]);

						if (ring[i][0]>1023) ring[i][0]=1023;
						if (ring[i][1]>1023) ring[i][1]=1023;
						if (ring[i][2]>1023) ring[i][2]=1023;
					}
				}
				if (fade[chan]>0.0){
					if (ring[next_i][0]+ring[next_i][1]+ring[next_i][2]==0){
						ring[next_i][0]=(uint16_t)((COLOR_CH[cur_colsch][chan][0])*fade[chan]);
						ring[next_i][1]=(uint16_t)((COLOR_CH[cur_colsch][chan][1])*fade[chan]);
						ring[next_i][2]=(uint16_t)((COLOR_CH[cur_colsch][chan][2])*fade[chan]);
					} else {
						ring[next_i][0]+=(uint16_t)((COLOR_CH[cur_colsch][chan][0])*fade[chan]);
						ring[next_i][1]+=(uint16_t)((COLOR_CH[cur_colsch][chan][1])*fade[chan]);
						ring[next_i][2]+=(uint16_t)((COLOR_CH[cur_colsch][chan][2])*fade[chan]);

						if (ring[next_i][0]>1023) ring[next_i][0]=1023;
						if (ring[next_i][1]>1023) ring[next_i][1]=1023;
						if (ring[next_i][2]>1023) ring[next_i][2]=1023;

					}
				}
				chan=6;//break;
			}
		}
	}
	DEBUG3_OFF;


	for (chan=0;chan<6;chan++){
		env_out_leds[chan][0]=(uint16_t)((COLOR_CH[cur_colsch][chan][0]) * ( (float)ENVOUT_PWM[chan]/4096.0) );
		env_out_leds[chan][1]=(uint16_t)((COLOR_CH[cur_colsch][chan][1]) * ( (float)ENVOUT_PWM[chan]/4096.0) );
		env_out_leds[chan][2]=(uint16_t)((COLOR_CH[cur_colsch][chan][2]) * ( (float)ENVOUT_PWM[chan]/4096.0) );
	}
#endif

	LEDDriver_set_LED_ring(ring, env_out_leds);
}



void display_scale(void){
	uint16_t ring[20][3];
	uint8_t i,j, chan;
	static uint8_t flash=0;
	uint16_t env_out_leds[6][3];


	uint8_t elacs[NUMSCALES][NUM_CHANNELS];
	uint8_t elacs_num[NUMSCALES];
	static uint8_t elacs_ctr[NUMSCALES]={0,0,0,0,0,0,0,0,0,0,0};

	//slow down the flashing
	if (flash++>3) flash=0;

	// Blank out the reverse-hash scale table
	for (i=0;i<NUMSCALES;i++) {
		elacs_num[i]=0;
		elacs[i][0]=99;
		elacs[i][1]=99;
		elacs[i][2]=99;
		elacs[i][3]=99;
		elacs[i][4]=99;
		elacs[i][5]=99;
	}

	// each entry in elacs[][] equals the number of channels
	for (i=0;i<NUM_CHANNELS;i++){
		elacs[motion_dst_scale[i]][elacs_num[motion_dst_scale[i]]] = i;
		elacs_num[motion_dst_scale[i]]++;
	}

	for (i=0;i<NUMSCALES;i++) {
		j=(i+(20-NUMSCALES/2)) % 20; //ring positions 15 16 17 18 19 0 1 2 3 4 5

		if (flash==0) {
			elacs_ctr[i]++;
			if (elacs_ctr[i] >= elacs_num[i]) elacs_ctr[i]=0;
		}

		//Blank out the channel if there are no entries
		if (elacs[i][0]==99){
			ring[j][0]=15;
			ring[j][1]=15;
			ring[j][2]=5;


		} else {
			ring[j][0]=COLOR_CH[cur_colsch][  elacs[i][ elacs_ctr[i] ]  ][0];
			ring[j][1]=COLOR_CH[cur_colsch][  elacs[i][ elacs_ctr[i] ]  ][1];
			ring[j][2]=COLOR_CH[cur_colsch][  elacs[i][ elacs_ctr[i] ]  ][2];

		}
	}


	// Show the scale bank settings

	for (i=0;i<NUM_CHANNELS;i++){
		j=13-i; //13, 12, 11, 10, 9, 8

		if (lock[i]!=1) {
			ring[j][0]=SCALE_BANK_COLOR[hover_scale_bank][0];
			ring[j][1]=SCALE_BANK_COLOR[hover_scale_bank][1];
			ring[j][2]=SCALE_BANK_COLOR[hover_scale_bank][2];
		} else {
			ring[j][0]=SCALE_BANK_COLOR[scale_bank[i]][0];
			ring[j][1]=SCALE_BANK_COLOR[scale_bank[i]][1];
			ring[j][2]=SCALE_BANK_COLOR[scale_bank[i]][2];
		}
	}

	// Blank out three spots to separate scale and bank
	//6
	ring[NUMSCALES/2+1][0]=0;
	ring[NUMSCALES/2+1][1]=0;
	ring[NUMSCALES/2+1][2]=0;

	//7
	ring[13-NUM_CHANNELS][0]=0;
	ring[13-NUM_CHANNELS][1]=0;
	ring[13-NUM_CHANNELS][2]=0;

	//14
	ring[19-NUMSCALES/2][0]=0;
	ring[19-NUMSCALES/2][1]=0;
	ring[19-NUMSCALES/2][2]=0;


	for (chan=0;chan<6;chan++){
		env_out_leds[chan][0]=(uint16_t)((COLOR_CH[cur_colsch][chan][0]) * ( (float)ENVOUT_PWM[chan]/4096.0) );
		env_out_leds[chan][1]=(uint16_t)((COLOR_CH[cur_colsch][chan][1]) * ( (float)ENVOUT_PWM[chan]/4096.0) );
		env_out_leds[chan][2]=(uint16_t)((COLOR_CH[cur_colsch][chan][2]) * ( (float)ENVOUT_PWM[chan]/4096.0) );
	}
	LEDDriver_set_LED_ring(ring, env_out_leds);
}

void display_spectral_readout(void){

	uint16_t ring[20][3];
	uint16_t env_out_leds[6][3];

	uint8_t i,chan;
	uint16_t t;
	uint32_t t32;

	for (i=0;i<20;i++){

		t32=((uint32_t)spectral_readout[i])>>15;
		if (t32>1024) t=1023;
		else if (t32<300) t=0;
		else t=(uint16_t)t32;

		ring[i][0]=t;
		ring[i][1]=t;
		ring[i][2]=t;

	}

	for (chan=0;chan<6;chan++){
		env_out_leds[chan][0]=(uint16_t)((COLOR_CH[cur_colsch][chan][0])*(float)ENVOUT_PWM[chan]);
		env_out_leds[chan][1]=(uint16_t)((COLOR_CH[cur_colsch][chan][1])*(float)ENVOUT_PWM[chan]);
		env_out_leds[chan][2]=(uint16_t)((COLOR_CH[cur_colsch][chan][2])*(float)ENVOUT_PWM[chan]);
	}
	LEDDriver_set_LED_ring(ring, env_out_leds);

}


inline void update_LED_ring(int16_t change_scale_mode){

	static uint32_t led_ring_update_ctr=0;

	if (led_ring_update_ctr++>3000 || flag_update_LED_ring){
		led_ring_update_ctr=0;
		flag_update_LED_ring=0;

		if (change_scale_mode){
			display_scale();
		} else {
			//if (display_spec==0)
				display_filter_rotation();
			//else
				//display_spectral_readout();
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

inline void update_lock_leds(void){
	uint8_t i;
	static uint8_t flash=0;
	static uint16_t lock_led_update_ctr=0;

	if (lock_led_update_ctr++>3000){
		lock_led_update_ctr=0;

		if (++flash>=16) flash=0;

		for (i=0;i<NUM_CHANNELS;i++){
			if (q_locked[i] && !flash){
				if (lock[i]) LOCKLED_OFF(i);
				else LOCKLED_ON(i);
			} else {
				if (lock[i]) LOCKLED_ON(i);
				else LOCKLED_OFF(i);
			}
		}
	}

}



void main(void)
{
	uint16_t rec_up,rec_down;
	uint16_t rotsw_up, rotsw_down;
	uint32_t rotsw_hold;
	uint32_t is_distinct;
	uint32_t play_down,play_up;

	uint16_t old_adc_buffer[NUM_ADCS];
	uint16_t old_potadc_buffer[NUM_ADC3S];
	uint8_t rotate_pot, rotate_shift;

	uint32_t din1_down=0,din2_down=0,din3_down=0,din4_down=0;
	uint32_t din1_up=0,din2_up=0,din3_up=0,din4_up=0;

	uint32_t rotary_state, old_rotary_state;
	uint8_t do_rotate_up, do_rotate_down;
	int8_t do_next_scale, do_prev_scale;


	uint8_t lock_test5;

	int16_t change_scale_mode=0;

	uint8_t user_turned_Q_pot=0;
	uint8_t user_turned_rotary=0;
	uint8_t dont_qunlock[NUM_CHANNELS];
	uint8_t already_handled_lock_release[NUM_CHANNELS];

	uint8_t just_switched_to_change_scale_mode=0;

	uint8_t rot_offset=0, old_rot_offset=0;

	int32_t t;
	int32_t t_scalecv, t_old_scalecv;
	float t_f;
	uint16_t temp_u16;
	uint32_t i,j;

	uint8_t color_assign=0;


	init_inouts();
	init_rotary();

	init_envout_pwm();

	Audio_Init();
	ADC1_Init((uint16_t *)adc_buffer);
	ADC3_Init((uint16_t *)potadc_buffer);

	Codec_Init(SAMPLERATE);
	delay();
	I2S_Block_Init();

	TIM6_Config();
	DAC_Ch1_NoiseConfig();


	LED_ON(LED_RING_OE); //actually turns the LED ring off
	LEDDriver_Init(5);
	for (i=0;i<20;i++)	LEDDriver_setRGBLED(i,0);
	LED_OFF(LED_RING_OE); //actually turns the LED ring on



	old_adc_buffer[0]=0xFFFF;
	old_adc_buffer[1]=0xFFFF;
	old_adc_buffer[SPREAD_ADC]=0xFFFF;

	for (i=0;i<NUM_CHANNELS;i++){
		scale[i]=6;
		motion_dst_scale[i]=scale[i];
		scale_bank[i]=0;
		rot_dir[i]=0;
	}

	note[0]=5;
	note[1]=6;
	note[2]=7;
	note[3]=8;
	note[4]=9;
	note[5]=10;
	motion_dst_note[0]=5;
	motion_dst_note[1]=6;
	motion_dst_note[2]=7;
	motion_dst_note[3]=8;
	motion_dst_note[4]=9;
	motion_dst_note[5]=10;

	flag_update_LED_ring=1;

	spread=(adc_buffer[SPREAD_ADC] >> 8) + 1;

	old_potadc_buffer[QPOT_ADC]=0xFFFF;

	I2S_Block_PlayRec();

	update_spread(1);

	while(1){

		check_errors();

		poll_switches();

		update_ENVOUT_PWM();

		update_LED_ring(change_scale_mode);

		update_lock_leds();

#ifdef ASSIGN_COLORS
		for (i=0;i<6;i++) {if (lock[i]==0) {color_assign=i;i=6;}}

		COLOR_CH[cur_colsch][color_assign][0] = adc_buffer[2]>>2;
		COLOR_CH[cur_colsch][color_assign][1] = adc_buffer[3]>>2;
		COLOR_CH[cur_colsch][color_assign][2] = adc_buffer[4]>>2;

//		COLOR_CH[cur_colsch][(color_assign+1) % 6][0] = adc_buffer[5]>>2;
//		COLOR_CH[cur_colsch][(color_assign+1) % 6][1] = adc_buffer[6]>>2;
//		COLOR_CH[cur_colsch][(color_assign+1) % 6][2] = adc_buffer[7]>>2;
#endif



		/*** SPREAD CV ***/
		update_spread(0);


		/**** LOCK JACKS ****/

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

/**** LOCK BUTTONS ****/


		for (i=0;i<6;i++){
			if (LOCKBUTTON(i)){
				lock_up[i]=1;
				if (lock_down[i]!=0
					&& lock_down[i]!=0xFFFFFFFF)  //don't wrap our counter!
					lock_down[i]++;

				if (lock_down[i]==200){ //first time we notice lock button is solidly down...
					lock_pressed[i]=1;
					user_turned_Q_pot=0;
					already_handled_lock_release[i]=0;
				}
				//check to see if it's been held down for a while, and the user hasn't turned the Q pot
				//if so, then we should unlock immediately, but not unlock the q_lock
				if (lock_down[i]>LOCK_BUTTON_QUNLOCK_HOLD_TIME && lock[i] && !user_turned_Q_pot) {
					//dont_qunlock[i]=1;
					lock[i]=0;
					LOCKLED_OFF(i);
					already_handled_lock_release[i]=1; //set this flag so that we don't do anything when the button is released
				}
				/*else dont_qunlock[i]=0;*/

			} else {
				lock_down[i]=1;
				if (lock_up[i]!=0) lock_up[i]++;
				if (lock_up[i]>200){ lock_up[i]=0;
					//Handle button release

					lock_pressed[i]=0;
					//scale_display_chan = figureout_which_channel_scale_to_show();

					if (!user_turned_Q_pot && !already_handled_lock_release[i]){ //only change lock state if user did not do a q_lock

						if (lock[i]==0){
							lock[i]=1;
							LOCKLED_ON(i);
						}
						else {
							lock[i]=0;
							/*if (!dont_qunlock[i]) */q_locked[i]=0;
							LOCKLED_OFF(i);
						}
					}

				}

			}
		}


/**** ROTARY BUTTON*****/

		if (ROTARY_SW){
			rotsw_up=1;
			if (rotsw_down!=0) rotsw_down++;

			//Handle long hold press: change colorscheme
			if (++rotsw_hold>200000){
				rotsw_hold=100000;
				cur_colsch=(cur_colsch+1) % NUM_COLORSCHEMES;
				change_scale_mode=0;
			}

			if (rotsw_down>200){rotsw_down=0;


				//Handle button press
				if (change_scale_mode==0) {

					change_scale_mode=1;
					just_switched_to_change_scale_mode=1;

					//scale_display_chan = figureout_which_channel_scale_to_show();

				} else {

					just_switched_to_change_scale_mode=0;
					//scale_display_chan = figureout_which_channel_scale_to_show();
				}

				user_turned_rotary=0;
				//user_pressed_rotary=1;

			}

		} else {
			rotsw_down=1;rotsw_hold=0;
			if (rotsw_up!=0) rotsw_up++;
			if (rotsw_up>200){ rotsw_up=0;

				//Handle button release

				for (i=0;i<NUM_CHANNELS;i++) {
					if (lock[i]!=1) {
						scale_bank[i]=hover_scale_bank; //Set all unlocked scale_banks to the same value
						//scale[i]=scale_bank_defaultscale[scale_bank[i]];
						//dest_scale_bank[i]=hover_scale_bank; //And then we need to handle a morph in filter.c
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


/**** ROTARY SHAFT *****/
		rotary_state=read_rotary();

		if (rotary_state==DIR_CW) {

			if (rotsw_up && !rotsw_down){ //HOVER BANK

				hover_scale_bank++;
				if (hover_scale_bank==NUMSCALEBANKS) hover_scale_bank=0; //wrap-around

				user_turned_rotary=1;
			}

			else if (!change_scale_mode){	//ROTATE UP

				rotate_up();

			}else{							//CHANGE SCALE
				do_next_scale=1;

			}
		}
		if (rotary_state==DIR_CCW) {

			if (rotsw_up && !rotsw_down){ //HOVER BANK

				if (hover_scale_bank==0) hover_scale_bank=NUMSCALEBANKS-1; //wrap-around
				else hover_scale_bank--;

				user_turned_rotary=1;

			} else if (!change_scale_mode){ //ROTATE DOWN

				rotate_down();

			}else {							//CHANGE SCALE
				do_prev_scale=1;
			}
		}


		if (do_next_scale){
			do_next_scale--;
			t_old_scalecv = change_scale_up(t_scalecv, t_old_scalecv);
		}

		if (do_prev_scale){
			do_prev_scale--;
			change_scale_down(t_scalecv, t_old_scalecv);
		}


/**** ROTATE TRIGGER JACKS *****/

		if (do_ROTUP){
			do_ROTUP=0;
			rotate_up();
		}


		if (do_ROTDOWN){
			do_ROTDOWN=0;
			rotate_down();
		}


/****** QVAL/QPOT ADC *****/

		//Check jack
		t=adc_buffer[QVAL_ADC];

		if (diff(t,old_adc_buffer[QVAL_ADC])>15){
			old_adc_buffer[QVAL_ADC]=adc_buffer[QVAL_ADC];
			qvalcv=adc_buffer[QVAL_ADC];
		}

		//Check pot

		if (diff(potadc_buffer[QPOT_ADC], old_potadc_buffer[QPOT_ADC]) > QPOT_MIN_CHANGE){

			old_potadc_buffer[QPOT_ADC]=potadc_buffer[QPOT_ADC];

			j=0;
			for (i=0;i<6;i++){
				if (lock_pressed[i]){ //if lock button is being held down, then q_lock the channel and assign its qval
					q_locked[i]=1;
					user_turned_Q_pot=1;

					qval[i]=potadc_buffer[QPOT_ADC];
					j++;
				}
			}

			//otherwise, if no lock buttons were held down, then change the qvalpot (which effects all non-q_locked channels)
			if (!j) qvalpot=potadc_buffer[QPOT_ADC];
		}

		for (i=0;i<NUM_CHANNELS;i++){
			if (!q_locked[i]){
				qval[i]=qvalcv + qvalpot;
				if (qval[i]>4095) qval[i]=4095;
			}
		}


/****** ROTATECV ADC *****/

		t=(int16_t)adc_buffer[ROTCV_ADC] - (int16_t)old_adc_buffer[ROTCV_ADC];
		if (t<(-100) || t>100){
			/*
			if (adc_buffer[ROTCV_ADC] > 4000 && old_adc_buffer[ROTCV_ADC] < 100){
				//Rising edge square wave could reset the rotation (# rotates up - # rotates down)

			}
			*/
			old_adc_buffer[ROTCV_ADC]=adc_buffer[ROTCV_ADC];

			rot_offset=adc_buffer[ROTCV_ADC]/205; //0..19

			if (rot_offset < old_rot_offset){
				do_rotate_down=old_rot_offset - rot_offset;
				do_rotate_up=0;
			} else if (rot_offset > old_rot_offset){
				do_rotate_up=rot_offset - old_rot_offset;
				do_rotate_down=0;
			}
			old_rot_offset = rot_offset;
		}


/****** SCALE_ADC *****/

		t=(int16_t)adc_buffer[SCALE_ADC] - (int16_t)old_adc_buffer[SCALE_ADC];
		if (t<(-100) || t>100){

			old_adc_buffer[SCALE_ADC]=adc_buffer[SCALE_ADC];

			t_scalecv=adc_buffer[SCALE_ADC]/409; //0..10

			if (t_scalecv < t_old_scalecv){
				do_prev_scale = t_old_scalecv - t_scalecv;

			} else if (t_scalecv > t_old_scalecv){
				do_next_scale = t_scalecv - t_old_scalecv;
			}
			t_old_scalecv = t_scalecv;
		}


/*** FREQ ADC***/
		t_f=(float)(adc_buffer[FREQNUDGE1_ADC]+adc_buffer[FM_135_ADC])/4096.0;
		if (t_f>1.0) t_f=1.0;

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

		if (mod_mode_246==246){
			freq_nudge[1]=t_f;
			freq_nudge[3]=t_f;
		} else {
			freq_nudge[1]=0.0;
			freq_nudge[3]=0.0;
		}

		freq_nudge[5]=t_f;


	} //end main loop


} //end main()

#ifdef  USE_FULL_ASSERT

#define assert_param(expr) ((expr) ? (void)0 : assert_failed((uint8_t *)__FILE__, __LINE__))

/**
  * @brief  Reports the name of the source file and the source line number
  *   where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t* file, uint32_t line)
{ 
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */

  /* Infinite loop */
  while (1)
  {
  }
}
#endif

#if 1
/* exception handlers - so we know what's failing */
void NMI_Handler(void)
{ 
	while(1){};
}

void HardFault_Handler(void)
{ 
	while(1){};
}

void MemManage_Handler(void)
{ 
	while(1){};
}

void BusFault_Handler(void)
{ 
	while(1){};
}

void UsageFault_Handler(void)
{ 
	while(1){};
}

void SVC_Handler(void)
{ 
	while(1){};
}

void DebugMon_Handler(void)
{ 
	while(1){};
}

void PendSV_Handler(void)
{ 
	while(1){};
}
#endif

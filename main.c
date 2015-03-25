/*
 * To Do
 * Spread CV should rotate across scales if rotate_to_next_scale is on
 *
 * Try single-variable Resonant BP (chrissy's link)
 * --freq_nudge could then be full range
 *
 * Figure out why low Q values are peaking
 *
 * Set ENVSPEED lpf values (it clips easily)
 *
 * Allow for different Q on each channel (hold lock while turning Q)
 *
 * Show CV on slider LEDs
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


uint8_t scale_bank_defaultscale[NUMSCALEBANKS]={4,4,6,5,9,5};

uint32_t g_error=0;

__IO uint16_t adc_buffer[NUM_ADCS];

uint8_t rotate=0;
uint8_t scale[NUM_CHANNELS];
uint8_t scale_bank[NUM_CHANNELS];
uint8_t dest_scale[NUM_CHANNELS];

uint8_t dest_scale_bank=0;
uint8_t hover_scale_bank=0;

uint8_t spread=99, test_spread=0, hys_spread=0,old_spread=1;

const uint32_t clip_led[6]={LED_CLIP1, LED_CLIP2, LED_CLIP3, LED_CLIP4, LED_CLIP5, LED_CLIP6};

uint8_t filter_assign_table[NUM_CHANNELS];
uint8_t dest_filter_assign_table[NUM_CHANNELS];
float rot_dir[6];

uint8_t lock[NUM_CHANNELS];
uint8_t lock_pressed[NUM_CHANNELS];
uint8_t lock_up[NUM_CHANNELS];
uint8_t lock_down[NUM_CHANNELS];

uint8_t flag_update_LED_ring=0;

float spectral_readout[NUM_FILTS];

extern uint8_t do_ROTUP;
extern uint8_t do_ROTDOWN;
extern uint8_t do_LOCK135;
extern uint8_t do_LOCK246;

extern uint16_t mod_mode_135, mod_mode_246;
extern uint16_t rotate_to_next_scale;

uint32_t qvalcv;

float freq_nudge[NUM_CHANNELS];

extern uint32_t ENVOUT_PWM[NUM_CHANNELS];



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

#define SPREAD_ADC_HYSTERESIS 75

extern float assign_fade[NUM_CHANNELS];

const float COLOR_CH[6][3]={
{0,0,1023},
{200,200,816},
{800,1000,800},
{1023,400,800},
{1000,500,202},
{800,200,0}
};

void update_filter_assign_LEDs(void){
	uint16_t ring[20][3];
	uint8_t i, next_i,chan=0;
	float inv_fade;

	for (i=0;i<20;i++){
		ring[i][0]=0;
		ring[i][1]=0;
		ring[i][2]=0;
	}

	for (i=0;i<20;i++){
		next_i=(i+1) % NUM_FILTS;
		for (chan=0;chan<6;chan++){
			if (filter_assign_table[chan]==i){
				inv_fade=1.0-assign_fade[chan];

				ring[i][0]=(uint16_t)((COLOR_CH[chan][0])*inv_fade);

				ring[i][1]=(uint16_t)((COLOR_CH[chan][1])*inv_fade);

				ring[i][2]=(uint16_t)((COLOR_CH[chan][2])*inv_fade);

				if (ring[next_i][0]+ring[next_i][1]+ring[next_i][2]==0){
					ring[next_i][0]=(uint16_t)((COLOR_CH[chan][0])*(assign_fade[chan]));
					ring[next_i][1]=(uint16_t)((COLOR_CH[chan][1])*(assign_fade[chan]));
					ring[next_i][2]=(uint16_t)((COLOR_CH[chan][2])*(assign_fade[chan]));
				}
				chan=6;
			}
		}
	}


	LEDDriver_set_LED_ring(ring);
}

const float SCALE_COLOR[NUMSCALES][3]={
		{0,0,1000},
		{0,200,800},
		{0,400,600},
		{0,600,400},
		{0,800,200},
		{0,1000,0},
		{200,800,200},
		{400,600,400},
		{600,400,600},
		{800,200,800},
		{1000,0,1000}

};

const float SCALE_BANK_COLOR[NUMSCALEBANKS][3]={

		{1000,1000,1000},
		{1000,0,0},
		{1000,1000,0},
		{0,1000,0},
		{0,800,800},
		{0,0,1000}

};

void display_scale(void){
	uint16_t ring[20][3];
	uint8_t i,j;
	static uint8_t flash=0;

	flash=1-flash;

	//First, turn all scale indicators on
	for (i=0;i<NUMSCALES;i++){
		j=(i+(20-NUMSCALES/2)) % 20;
		ring[j][0]=SCALE_COLOR[i][0];
		ring[j][1]=SCALE_COLOR[i][1];
		ring[j][2]=SCALE_COLOR[i][2];
	}

	//Then, if we're flashing, turn off all scales that are active on some channel
	if (flash){
		for (i=0;i<NUM_CHANNELS;i++){
			j=(scale[i]+(20-NUMSCALES/2)) % 20;
			ring[j][0]=0;
			ring[j][1]=0;
			ring[j][2]=0;
		}
	}

	ring[NUMSCALES/2+1][0]=0;
	ring[NUMSCALES/2+1][1]=0;
	ring[NUMSCALES/2+1][2]=0;

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
	ring[13-NUM_CHANNELS][0]=0;
	ring[13-NUM_CHANNELS][1]=0;
	ring[13-NUM_CHANNELS][2]=0;

	ring[19-NUMSCALES/2][0]=0;
	ring[19-NUMSCALES/2][1]=0;
	ring[19-NUMSCALES/2][2]=0;


	LEDDriver_set_LED_ring(ring);
}

void display_spectral_readout(void){

	uint16_t ring[20][3];
	uint8_t i;
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

	LEDDriver_set_LED_ring(ring);

}

/*
uint8_t figureout_which_channel_scale_to_show(void){
	uint8_t i,chan;

	for (i=0;i<NUM_CHANNELS;i++){
		if (lock_pressed[i]){ //display the first channel that has a lock button held down
			return (i);
			break;
		}
	}

	for (i=0;i<NUM_CHANNELS;i++){
		chan=i;
		if (scale_bank[i]==unlocked_scale_bank && lock[i]!=1){ //or display the first channel that's unlocked and in the unlocked bank
			return(i);
			break;
		}
	}
	//or display the last channel by default
	return(chan);
}
*/

inline uint8_t num_locks_pressed(void){
	uint8_t i,j;
	for (i=0,j=0;i<NUM_CHANNELS;i++){
		if (lock_pressed[i]) j++;
	}
	return j;
}




void main(void)
{
	uint16_t rec_up,rec_down;
	uint16_t rotsw_up, rotsw_down;
	uint32_t is_distinct;
	uint32_t play_down,play_up;

	uint16_t old_adc_buffer[NUM_ADCS];
	uint8_t rotate_pot, rotate_shift;

	uint32_t din1_down=0,din2_down=0,din3_down=0,din4_down=0;
	uint32_t din1_up=0,din2_up=0,din3_up=0,din4_up=0;

	uint32_t rotary_state, old_rotary_state;
	uint8_t do_rotate_up, do_rotate_down;

	uint32_t led_ring_update_ctr=0;

	uint8_t lock_test5;

	int16_t change_scale_mode=0;

	uint8_t user_turned_rotary=0;
	uint8_t user_pressed_rotary=0;
	uint8_t just_switched_to_change_scale_mode=0;

	uint8_t force_spread_update=0;

	uint8_t rot_offset=0, old_rot_offset=0;

	int32_t t;
	float t_f;
	uint16_t temp_u16;
	uint32_t i,j;


	init_inouts();
	init_rotary();

	init_envout_pwm();

	Audio_Init();
	ADC1_Init((uint16_t *)adc_buffer);
	Codec_Init(SAMPLERATE);
	delay();
	I2S_Block_Init();
	I2S_Block_PlayRec();

	TIM6_Config();
	DAC_Ch1_NoiseConfig();


	LED_ON(LED_RING_OE); //actually turns the LED ring off
	LEDDriver_Init(4);
	for (i=0;i<20;i++)	LEDDriver_setRGBLED(i,0);
	LED_OFF(LED_RING_OE); //actually turns the LED ring on



	old_adc_buffer[0]=0xFFFF;
	old_adc_buffer[1]=0xFFFF;
	old_adc_buffer[SPREAD_ADC]=0xFFFF;

	filter_assign_table[0]=1;
	filter_assign_table[1]=2;
	filter_assign_table[2]=3;
	filter_assign_table[3]=4;
	filter_assign_table[4]=5;
	filter_assign_table[5]=6;
	dest_filter_assign_table[0]=5;
	dest_filter_assign_table[1]=7;
	dest_filter_assign_table[2]=9;
	dest_filter_assign_table[3]=11;
	dest_filter_assign_table[4]=13;
	dest_filter_assign_table[5]=15;

	flag_update_LED_ring=1;

	spread=(adc_buffer[SPREAD_ADC] >> 8) + 1;
	force_spread_update=1;

	for (i=0;i<NUM_CHANNELS;i++){
		scale[i]=6;
		dest_scale[i]=scale[i];
		scale_bank[i]=0;
	}

	while(1){

		check_errors();

		update_ENVOUT_PWM();

		if (led_ring_update_ctr++>6000 || flag_update_LED_ring){
			led_ring_update_ctr=0;
			flag_update_LED_ring=0;

			if (change_scale_mode){
				display_scale();
			} else {
				//if (display_spec==0)
					update_filter_assign_LEDs();
				//else
					//display_spectral_readout();

			}
		}


		poll_switches();


/*** SPREAD CV ***/

		test_spread=(adc_buffer[SPREAD_ADC] >> 8) + 1; //0-4095 to 1-16

		if (test_spread < spread){
			if (adc_buffer[SPREAD_ADC] <= (4095-SPREAD_ADC_HYSTERESIS))
				temp_u16 = adc_buffer[SPREAD_ADC] + SPREAD_ADC_HYSTERESIS;
			else
				temp_u16 = 4095;

			hys_spread = (temp_u16 >> 8) + 1;
			old_spread=spread;

		} else if (test_spread > spread){
			if (adc_buffer[SPREAD_ADC] > SPREAD_ADC_HYSTERESIS)
				temp_u16 = adc_buffer[SPREAD_ADC] - SPREAD_ADC_HYSTERESIS;
			else
				temp_u16 = 0;

			hys_spread = (temp_u16 >> 8) + 1;
			old_spread=spread;

		} else {
			hys_spread=0xFF; //adc has not changed, do nothing
		}

		if (hys_spread == test_spread || force_spread_update){
			force_spread_update=0;

			spread=test_spread;
			if (spread>old_spread)
				t_f=1;
			else t_f=-1;

			if (lock[5]==1) lock_test5=dest_filter_assign_table[5];
			else lock_test5=99;

			//Assign [0] and check for duplicate placement over 2 & lock5
			if (lock[0]!=1){
				if ((NUM_FILTS+dest_filter_assign_table[2])<(spread*2)) dest_filter_assign_table[0] = (NUM_FILTS*2)+dest_filter_assign_table[2]-(spread*2);
				else if (dest_filter_assign_table[2]<(spread*2)) dest_filter_assign_table[0] = NUM_FILTS+dest_filter_assign_table[2]-(spread*2);
				else dest_filter_assign_table[0]=dest_filter_assign_table[2]-(spread*2);
				rot_dir[0]=-1.0*t_f;

				while (dest_filter_assign_table[0]==lock_test5
						|| dest_filter_assign_table[0]==dest_filter_assign_table[2])
					dest_filter_assign_table[0]=(dest_filter_assign_table[0]+1) % NUM_FILTS;

			}

			//Assign [1] and check for duplicate placement over 0, 2 & lock5
			if (dest_filter_assign_table[2]<spread) dest_filter_assign_table[1] = NUM_FILTS+dest_filter_assign_table[2]-spread;
			else dest_filter_assign_table[1]=dest_filter_assign_table[2]-spread;
			rot_dir[1]=-1.0*t_f;
			while (				dest_filter_assign_table[1]==dest_filter_assign_table[0]
			       				|| dest_filter_assign_table[1]==dest_filter_assign_table[2]
								|| dest_filter_assign_table[1]==lock_test5)
				dest_filter_assign_table[1]=(dest_filter_assign_table[1]+1) % NUM_FILTS;



			//Keep [2] stationary
			rot_dir[2]=-1.0*t_f;

			//Assign [3] and check for duplicate placement over 0,1,2 & lock5
			dest_filter_assign_table[3]=(dest_filter_assign_table[2]+spread) % NUM_FILTS;
			rot_dir[3]=t_f;
			while (				dest_filter_assign_table[3]==dest_filter_assign_table[0]
			       				|| dest_filter_assign_table[3]==dest_filter_assign_table[1]
								|| dest_filter_assign_table[3]==dest_filter_assign_table[2]
								|| dest_filter_assign_table[3]==lock_test5)
				dest_filter_assign_table[3]=(dest_filter_assign_table[3]+1) % NUM_FILTS;


			//Assign [4] and check for duplicate placement over 0,1,2,3 & any locks
			dest_filter_assign_table[4]=(dest_filter_assign_table[2]+(spread*2)) % NUM_FILTS;
			rot_dir[4]=t_f;
			while (				dest_filter_assign_table[4]==dest_filter_assign_table[0]
								|| dest_filter_assign_table[4]==dest_filter_assign_table[1]
								|| dest_filter_assign_table[4]==dest_filter_assign_table[2]
								|| dest_filter_assign_table[4]==dest_filter_assign_table[3]
								|| dest_filter_assign_table[4]==lock_test5)
				dest_filter_assign_table[4]=(dest_filter_assign_table[4]+1) % NUM_FILTS;


			//Assign [5] and check for duplicate placement over 0,1,2,3,4
			if (lock[5]!=1) {
				dest_filter_assign_table[5]=(dest_filter_assign_table[2]+(spread*3)) % NUM_FILTS;
				rot_dir[5]=t_f;

				while (				dest_filter_assign_table[5]==dest_filter_assign_table[0]
				       				|| dest_filter_assign_table[5]==dest_filter_assign_table[1]
				       			    || dest_filter_assign_table[5]==dest_filter_assign_table[2]
				                    || dest_filter_assign_table[5]==dest_filter_assign_table[3]
				                    || dest_filter_assign_table[5]==dest_filter_assign_table[4]
									)
					dest_filter_assign_table[5]=(dest_filter_assign_table[5]+1) % NUM_FILTS;

			}

			//update_filter_assign_LEDs();
		}


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
				if (lock_down[i]!=0) lock_down[i]++;
				if (lock_down[i]>200){
					//Handle button press
					lock_pressed[i]=1;
					user_pressed_rotary=0;
				}
			} else {
				lock_down[i]=1;
				if (lock_up[i]!=0) lock_up[i]++;
				if (lock_up[i]>200){ lock_up[i]=0;
					//Handle button release

					lock_pressed[i]=0;
					//scale_display_chan = figureout_which_channel_scale_to_show();

					if (!user_pressed_rotary){ 				//only change lock state if user did not click
						if (lock[i]==0){
							lock[i]=1;
							LOCKLED_ON(i);
						}
						else {
							lock[i]=0;
							LOCKLED_OFF(i);
						}
					} else { 							//if user held lock and clicked over to display the locked scale, then exit change scale mode
						if (change_scale_mode==2)
							change_scale_mode=0;
					}

				}

			}
		}


/**** ROTARY BUTTON*****/

		if (ROTARY_SW){
			rotsw_up=1;
			if (rotsw_down!=0) rotsw_down++;
			if (rotsw_down>200){rotsw_down=0;
				//Handle button press
				if (change_scale_mode==0) {

					if (num_locks_pressed()>0)
						change_scale_mode=2;
					else
						change_scale_mode=1;

					just_switched_to_change_scale_mode=1;
					//scale_display_chan = figureout_which_channel_scale_to_show();
				}

				else if (change_scale_mode==1) {
					if (num_locks_pressed()>0){
						change_scale_mode=2;
						just_switched_to_change_scale_mode=1;
					} else
						just_switched_to_change_scale_mode=0;
					//scale_display_chan = figureout_which_channel_scale_to_show();
				}

				else if (change_scale_mode==2){
					change_scale_mode=0;
					just_switched_to_change_scale_mode=1;
					//scale_display_chan = figureout_which_channel_scale_to_show();
				}

				user_turned_rotary=0;
				user_pressed_rotary=1;

			}

		} else {
			rotsw_down=1;
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

				if (change_scale_mode==1 && !just_switched_to_change_scale_mode && !user_turned_rotary) {
					change_scale_mode=0;
					just_switched_to_change_scale_mode=0;
				}
				else if (change_scale_mode==2 && !just_switched_to_change_scale_mode && !user_turned_rotary){
					change_scale_mode=0;
					just_switched_to_change_scale_mode=0;
				}
			}
		}


/**** ROTARY SHAFT *****/
		rotary_state=read_rotary();

		if (rotary_state==DIR_CW) {

			if (rotsw_up && !rotsw_down){ //CHANGE SCALE BANK

				hover_scale_bank++;
				if (hover_scale_bank==NUMSCALEBANKS) hover_scale_bank=0; //wrap-around

				user_turned_rotary=1;
			}

			else if (!change_scale_mode){	//ROTATE UP
				do_rotate_up=1;

			}else{							//CHANGE SCALE

				for (i=0;i<NUM_CHANNELS;i++) {
					if (lock[i]!=1) {
						if (scale[i]<(NUMSCALES-1)) {
							scale[i]++; //no wrap-around
							if (dest_scale[i]<(NUMSCALES-1))
								dest_scale[i]++;//dest_scale[i]=scale[i];?
						}
						scale_bank_defaultscale[scale_bank[i]]=scale[i];
					}
				}
			}
		}
		if (rotary_state==DIR_CCW) {

			if (rotsw_up && !rotsw_down){ //CHANGE SCALE BANK

				if (hover_scale_bank==0) hover_scale_bank=NUMSCALEBANKS-1; //wrap-around
				else hover_scale_bank--;

				user_turned_rotary=1;

			} else if (!change_scale_mode){ //ROTATE DOWN
				do_rotate_down=1;

			}else {							//CHANGE SCALE
				for (i=0;i<NUM_CHANNELS;i++) {

					if (lock[i]!=1) {
						if (scale[i]>0)	{
							scale[i]--; //no wrap-around
							if (dest_scale[i]>0) dest_scale[i]--;//=scale[i];?
						}
						scale_bank_defaultscale[scale_bank[i]]=scale[i];
					}
				}
			}
		}

/**** ROTATE TRIGGERS *****/

		if (do_ROTUP){
			do_ROTUP=0;
			do_rotate_up=1;
		}


		if (do_ROTDOWN){
			do_ROTDOWN=0;
			do_rotate_down=1;
		}

/***** HANDLE ROTATE UP *****/

		if (do_rotate_up){

			while (do_rotate_up--){
				for (i=0;i<NUM_CHANNELS;i++){ //takes 4us
					//Don't change locked filters
					if (lock[i]!=1){

						//Find a distinct value, shifting upward if necessary
						is_distinct=0;
						while (!is_distinct){
							//dest_filter_assign_table[i]=(dest_filter_assign_table[i]+1) % NUM_FILTS;
							//Loop it around to the bottom of the scale, or to bottom of the next scale if we're in RANGE BANK mode
							if (dest_filter_assign_table[i]==(NUM_FILTS-1)){
								dest_filter_assign_table[i]=0;
								if (rotate_to_next_scale) {
									dest_scale[i]=(scale[i]+1) % NUMSCALES;
								}
							} else
								dest_filter_assign_table[i]++;

							for (is_distinct=1,j=0;j<NUM_CHANNELS;j++){
								if (i!=j && dest_filter_assign_table[i]==dest_filter_assign_table[j] && lock[j]==1)
									is_distinct=0;
							}
						}
					}
				}
			}
			rot_dir[0]=1; //CW
			rot_dir[1]=1; //CW
			rot_dir[2]=1; //CW
			rot_dir[3]=1; //CW
			rot_dir[4]=1; //CW
			rot_dir[5]=1; //CW

			do_rotate_up=0;
		}

/***** HANDLE ROTATE DOWN *****/

		if (do_rotate_down){

			while (do_rotate_down--){
				for (i=0;i<NUM_CHANNELS;i++){
					//Don't change locked filters
					if (lock[i]!=1){

						//Find a distinct value, shifting downward if necessary
						is_distinct=0;
						while (!is_distinct){

							if (dest_filter_assign_table[i]==0) {
								dest_filter_assign_table[i] = NUM_FILTS-1;
								if (rotate_to_next_scale){
									if (scale[i]==0) dest_scale[i]=NUMSCALES-1;
									else dest_scale[i]=scale[i]-1;
								}
							}
							else
								dest_filter_assign_table[i]--;

							for (is_distinct=1,j=0;j<NUM_CHANNELS;j++){
								if (i!=j && dest_filter_assign_table[i]==dest_filter_assign_table[j] && lock[j]==1)
									is_distinct=0;
							}
						}
					}
				}
			}
			rot_dir[0]=-1; //CCW
			rot_dir[1]=-1; //CCW
			rot_dir[2]=-1; //CCW
			rot_dir[3]=-1; //CCW
			rot_dir[4]=-1; //CCW
			rot_dir[5]=-1; //CCW

			do_rotate_down=0;
		}


/****** QVAL ADC *****/

		t=adc_buffer[QVAL_ADC] - old_adc_buffer[QVAL_ADC];
		if (t<(-15) || t>15){
			old_adc_buffer[QVAL_ADC]=adc_buffer[QVAL_ADC];

			//Handle new pot0 value
			qvalcv=adc_buffer[QVAL_ADC];

		}

/****** ROTATECV ADC *****/

		t=adc_buffer[SCALE_ADC] - old_adc_buffer[SCALE_ADC];
		if (t<(-100) || t>100){
			/*
			if (adc_buffer[ROTRESET_ADC] > 4000 && old_adc_buffer[ROTRESET_ADC] < 100){
				//Rising edge square wave

			}
			*/
			old_adc_buffer[SCALE_ADC]=adc_buffer[SCALE_ADC];

			rot_offset=adc_buffer[SCALE_ADC]/205; //0..19

			if (rot_offset < old_rot_offset){
				do_rotate_down=old_rot_offset - rot_offset;
			} else if (rot_offset > old_rot_offset){
				do_rotate_up=rot_offset - old_rot_offset;
			}
			old_rot_offset = rot_offset;
		}



/****** SCALE_ADC *****/

		t=adc_buffer[SCALE_ADC] - old_adc_buffer[SCALE_ADC];
		if (t<(-MIN_SCALE_ADC_CHANGE) || t>MIN_SCALE_ADC_CHANGE){
			old_adc_buffer[SCALE_ADC]=adc_buffer[SCALE_ADC];

			//Handle new value
			t=adc_buffer[SCALE_ADC]/409;
			for (i=0;i<NUM_CHANNELS;i++) {
				if (lock[i]!=1) {
					//scale_cv[i]=t;
					//if (scale_cv[i]>=NUMSCALES) scale_cv[i]=NUMSCALES-1;
#ifdef USINGBREAKOUT
					scale[i]=t;
					if (scale[i]>=NUMSCALES) scale[i]=NUMSCALES-1;
#endif

				}

			}
		}

/*** FREQ ADC***/

		if (mod_mode_135==135){
			freq_nudge[2]=(float)adc_buffer[FM_135_ADC]/4096.0;
			freq_nudge[4]=(float)adc_buffer[FM_135_ADC]/4096.0;
		}
		freq_nudge[0]=(float)(adc_buffer[FREQNUDGE1_ADC]+adc_buffer[FM_135_ADC])/4096.0;
		if (freq_nudge[0]>1.0) freq_nudge[0]=1.0;

		if (mod_mode_246==246){
			freq_nudge[1]=(float)adc_buffer[FM_246_ADC]/4096.0;
			freq_nudge[3]=(float)adc_buffer[FM_246_ADC]/4096.0;
		}
		freq_nudge[5]=(float)(adc_buffer[FREQNUDGE6_ADC]+adc_buffer[FM_246_ADC])/4096.0;
		if (freq_nudge[5]>1.0) freq_nudge[5]=1.0;


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

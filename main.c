/*
 * To-Do:
 *
 * How to do Spread so it doesn't just start with the first and count up? Perhaps start with the 1/2 between the middle two, and count down and up?
 *
 * Rotate Glide mode (bend/fade?)
 *
 * Rotate Glide ping?
 *
 * CV Morph is still weird.. maybe four-position switch? Or two SPDTs? Would be nice to flip it off (no lag)
 * Or ping/linear switch? Full envelope controls (skew/time or A/D)
 *
 *
 */


#include "stm32f4xx.h"
//#include "stm32f4xx_dac.h"
//#include "stm32f4xx_tim.h"

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



uint8_t memfull=0;
uint8_t num_samples=0;
uint32_t cur_rec_addr=0;
uint32_t cur_play_addr=0;

uint32_t g_error=0;

uint8_t write_out_i=0;
uint8_t write_in_i=0;

uint8_t read_out_i=0;
uint8_t read_in_i=0;

uint8_t is_recording=0;
volatile uint8_t play_state;

__IO uint16_t adc_buffer[NUM_ADCS];

uint8_t rotate=0;
int8_t scale;
int8_t scale_bank;

uint8_t spread=99, test_spread=0, hys_spread=0,old_spread=1;

float adc_lag_attack=0;
float adc_lag_decay=0;

uint32_t ENVOUT_PWM[NUM_ACTIVE_FILTS];
uint32_t envout_mode=PRE;
const uint32_t clip_led[6]={LED_CLIP1, LED_CLIP2, LED_CLIP3, LED_CLIP4, LED_CLIP5, LED_CLIP6};

uint8_t filter_assign_table[NUM_ACTIVE_FILTS];
uint8_t dest_filter_assign_table[NUM_ACTIVE_FILTS];
float midpt,halfspread;
float rot_dir[6];
uint8_t blend_mode;

uint8_t lock[NUM_ACTIVE_FILTS];

uint8_t flag_update_LED_ring=0;

float spectral_readout[NUM_FILTS];
uint8_t strike=0;


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



extern float assign_fade[NUM_ACTIVE_FILTS];

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
		{500,1000,0},
		{0,1000,500},
		{0,600,600}

};

void display_scale(void){
	uint16_t ring[20][3];
	uint8_t i,j;
	static uint8_t flash=0;

	flash=1-flash;
	for (i=0;i<NUMSCALES;i++){
		j=(i+(20-NUMSCALES/2)) % 20;
		if (scale==i && flash){
			ring[j][0]=0;
			ring[j][1]=0;
			ring[j][2]=0;
		} else {
			ring[j][0]=SCALE_COLOR[i][0];
			ring[j][1]=SCALE_COLOR[i][1];
			ring[j][2]=SCALE_COLOR[i][2];
		}
	}

	ring[NUMSCALES/2+1][0]=0;
	ring[NUMSCALES/2+1][1]=0;
	ring[NUMSCALES/2+1][2]=0;

	for (i=NUMSCALES/2+2;i<(19-NUMSCALES/2);i++){
		ring[i][0]=SCALE_BANK_COLOR[scale_bank][0];
		ring[i][1]=SCALE_BANK_COLOR[scale_bank][1];
		ring[i][2]=SCALE_BANK_COLOR[scale_bank][2];
	}
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


void main(void)
{
	uint16_t rec_up,rec_down,button3_up,button3_down,button4_up,button4_down;
	uint16_t rotsw_up, rotsw_down;
	uint32_t i,j;
	uint32_t play_down,play_up;
	int32_t t;
	uint16_t old_adc_buffer[NUM_ADCS];
	uint8_t rotate_pot, rotate_shift;
	uint32_t din1_down=0,din2_down=0,din3_down=0,din4_down=0;
	uint32_t din1_up=0,din2_up=0,din3_up=0,din4_up=0;

	uint32_t old_cvlag=0xFFFF;

	uint32_t rotary_state, old_rotary_state;
	uint8_t do_rotate_up, do_rotate_down;

	uint16_t temp_u16;

	uint32_t led_ring_update_ctr=0;

	uint8_t lock_test5;

	int16_t change_scale_mode=0;
	uint8_t user_turned_rotary=0;
	uint8_t just_switched_to_change_scale_mode=0;

	//uint16_t display_spec=0;

	uint8_t force_spread_update=0;


	float lag_amount;
	float t_f;

//	uint8_t buttons_down[NUM_BUTTONS];
//	uint8_t buttons_up[NUM_BUTTONS];

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

	scale=6;
	scale_bank=0;

	spread=(adc_buffer[SPREAD_ADC] >> 8) + 1;
	force_spread_update=1;

	while(1){

		check_errors();

		update_ENVOUT_PWM();

		if (led_ring_update_ctr++>3000 || flag_update_LED_ring){
			led_ring_update_ctr=0;
			flag_update_LED_ring=0;

			if (change_scale_mode){
				//show_scale--;
				display_scale();
			} else {
				//if (display_spec==0)
					update_filter_assign_LEDs();
				//else
					//display_spectral_readout();

			}
		}


		if (lock[0]==1) LED_ON(LED_LOCK0);
		else LED_OFF(LED_LOCK0);

		if (lock[5]==1) LED_ON(LED_LOCK5);
		else LED_OFF(LED_LOCK5);


		/*** Read controls ***/

		//ENV MODE
		if (ENV_MODE){
				envout_mode=0;
		} else {
				envout_mode=1;
		}

		//SPREADCV

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

			if (lock[5]) lock_test5=dest_filter_assign_table[5];
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


		if (LOCK0){
			button3_up=1;
			if (button3_down!=0) button3_down++;
			if (button3_down>100000){
				//Handle button held down for a second
				button3_down=0;
				lock[0]=0; 							//clear the lock
				LED_OFF(LED_LOCK0);
			}
		} else {
			button3_down=1;
			if (button3_up!=0) button3_up++;
			if (button3_up>200){ button3_up=0;

				//Handle button release
				if (lock[0]==0 || lock[0]==2){ 	//cleared or offset -> locked
					lock[0]=1;
					LED_ON(LED_LOCK0);
				}
				else if (lock[0]==1) {				//locked->offset
					lock[0]=2;
					LED_OFF(LED_LOCK0);
				}

			}
		}

		if (LOCK5){
			button4_up=1;
			if (button4_down!=0) button4_down++;
			if (button4_down>200){ button4_down=0;
				//Handle button press

			}
		} else {
			button4_down=1;
			if (button4_up!=0) button4_up++;
			if (button4_up>200){ button4_up=0;

				//Handle button release
				if (lock[5]==0){
					lock[5]=1; //cleared->locked
					LED_ON(LED_LOCK5);
				}
				else if (lock[5]==1){
					lock[5]=0; //locked->unlocked
					LED_OFF(LED_LOCK5);
				}

			}
		}




/**** ROTATE ROTARY BUTTON*****/

		if (ROTARY_SW){
			rotsw_up=1;
			if (rotsw_down!=0) rotsw_down++;
			if (rotsw_down>4){rotsw_down=0;
				//Handle button press
				if (change_scale_mode==0) {
					change_scale_mode=1;
					just_switched_to_change_scale_mode=1;
				}
				else if (change_scale_mode==1) just_switched_to_change_scale_mode=0;

				user_turned_rotary=0;

			}

		} else {
			rotsw_down=1;
			if (rotsw_up!=0) rotsw_up++;
			if (rotsw_up>4){ rotsw_up=0;
				//Handle button release
				if (change_scale_mode==1 && !just_switched_to_change_scale_mode && !user_turned_rotary) {
					change_scale_mode=0;
					user_turned_rotary=0;
					just_switched_to_change_scale_mode=0;
				}
			}
		}


/**** ROTATE ROTARY *****/
		rotary_state=read_rotary();

		if (rotary_state==DIR_CW) {

			if (rotsw_up && !rotsw_down){ //Rotary switch has been processed as being pressed
				scale_bank++;
				if (scale_bank==NUMSCALEBANKS) scale_bank=0;
				user_turned_rotary=1;
			}
			else if (!change_scale_mode){
				do_rotate_up=1;
			}else{
				scale++;
				if (scale>=NUMSCALES) scale=NUMSCALES-1;
			}
		}
		if (rotary_state==DIR_CCW) {

			if (rotsw_up && !rotsw_down){ //Rotary switch is has been processed as being pressed
				if (scale_bank==0) scale_bank=NUMSCALEBANKS-1;
				else scale_bank--;
				user_turned_rotary=1;

			} else if (!change_scale_mode){
				do_rotate_down=1;
			}else {
				scale--;
				if (scale<0) scale=0;
			}
		}


/**** ROTATE TRIGGERS *****/

		if (!ROTUP){
			din1_up=1;
			if (din1_down!=0) din1_down++;
			if (din1_down>4){din1_down=0;
				//Handle button press
				do_rotate_up=1;
			}
		} else {
			din1_down=1;
			if (din1_up!=0) din1_up++;
			if (din1_up>4){ din1_up=0;
				//Handle button release
			}
		}


		if (!ROTDOWN){
			do_rotate_down=0;
			din2_up=1;
			if (din2_down!=0) din2_down++;
			if (din2_down>4){din2_down=0;
				//Handle button press
				do_rotate_down=1;
			}
		} else {
			din2_down=1;
			if (din2_up!=0) din2_up++;
			if (din2_up>4){ din2_up=0;
				//Handle button release
			}
		}

/***** HANDLE ROTATE UP *****/

		if (do_rotate_up){
			do_rotate_up=0;

			for (i=0;i<NUM_ACTIVE_FILTS;i++){ //takes 4us
				//Don't change locked filters
				if (lock[i]!=1){
					dest_filter_assign_table[i]=(dest_filter_assign_table[i]+1) % NUM_FILTS;
					//Force unique values, shifting up
					for (j=0;j<NUM_ACTIVE_FILTS;j++){
						//compare current element [i] to all elements that are locked or that we have already assigned
						if ((lock[j]==1 || j<i) && j!=i){
							while (dest_filter_assign_table[i]==dest_filter_assign_table[j])
								dest_filter_assign_table[i]=(dest_filter_assign_table[i]+1) % NUM_FILTS;
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
			//update_filter_assign_LEDs();
		}

/***** HANDLE ROTATE DOWN *****/

		if (do_rotate_down){
			do_rotate_down=0;

			for (i=0;i<NUM_ACTIVE_FILTS;i++){
				//Don't change locked filters
				if (lock[i]!=1){
					if (dest_filter_assign_table[i]==0)
						dest_filter_assign_table[i] = NUM_FILTS-1;
					else
						dest_filter_assign_table[i] = dest_filter_assign_table[i] - 1;
					//Force unique values, shifting down
					for (j=0;j<NUM_ACTIVE_FILTS;j++){
						//compare current element [i] to all elements that are locked or that we have already assigned
						if ((lock[j]==1 || j<i) && j!=i){
							while (dest_filter_assign_table[i]==dest_filter_assign_table[j]){
								if (dest_filter_assign_table[i]==0)
									dest_filter_assign_table[i] = NUM_FILTS-1;
								else
									dest_filter_assign_table[i] = dest_filter_assign_table[i] - 1;
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
			//update_filter_assign_LEDs();
		}

/****** CV LAG Switches*****/
		t=CVLAG;
		if (old_cvlag!=t){
			old_cvlag=t;


			if (t){
				adc_lag_attack=LAG_ATTACK;
				adc_lag_decay=LAG_DECAY;
			}else{
				adc_lag_attack=0;
				adc_lag_decay=0;
			}
		}




/****** CV LAG ADC *****/


		t=adc_buffer[QVAL_ADC] - old_adc_buffer[QVAL_ADC];
		if (t<(-MIN_ADC_CHANGE) || t>MIN_ADC_CHANGE){
			old_adc_buffer[QVAL_ADC]=adc_buffer[QVAL_ADC];

			//Handle new pot0 value

		}


/****** SCALE_ADC *****/

		t=adc_buffer[SCALE_ADC] - old_adc_buffer[SCALE_ADC];
		if (t<(-MIN_SCALE_ADC_CHANGE) || t>MIN_SCALE_ADC_CHANGE){
			old_adc_buffer[SCALE_ADC]=adc_buffer[SCALE_ADC];

			//Handle new value
			scale=adc_buffer[SCALE_ADC]/409;
			if (scale>NUMSCALES) scale=NUMSCALES;
		}




	}
}

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

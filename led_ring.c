#include "stm32f4xx.h"
#include "led_ring.h"
#include "led_driver.h"
#include "adc.h"
#include "params.h"
#include "globals.h"
#include "inouts.h"

extern float channel_level[NUM_CHANNELS];
extern uint8_t lock[NUM_CHANNELS];
extern uint8_t lock_pressed[NUM_CHANNELS];
extern uint8_t lock_up[NUM_CHANNELS];
extern uint8_t already_handled_lock_release[NUM_CHANNELS];

extern float motion_morphpos[NUM_CHANNELS];
extern int8_t motion_fadeto_note[NUM_CHANNELS];
extern int8_t motion_scale_dest[NUM_CHANNELS];

extern uint8_t note[NUM_CHANNELS];
extern uint8_t scale_bank[NUM_CHANNELS];
extern uint8_t hover_scale_bank;

extern uint32_t ENVOUT_PWM[NUM_CHANNELS];

extern __IO uint16_t potadc_buffer[NUM_ADCS];

extern uint8_t assign_color_mode;
extern uint8_t select_colors_mode;

uint8_t flag_update_LED_ring=0;
float spectral_readout[NUM_FILTS];

#define FLASHADDR_colorscheme 0x080E0000

//Default values, that should be overwritten when reading flash
uint8_t cur_colsch=0;
uint32_t u_NUM_COLORSCHEMES=16;
float COLOR_CH[16][6][3]={
		{{0, 0, 761}, {0, 770, 766}, {0, 766, 16}, {389, 383, 387}, {763, 154, 0}, {766, 0, 112}},
		{{0, 0, 761}, {0, 780, 766}, {768, 767, 764}, {493, 768, 2}, {763, 154, 0}, {580, 65, 112}},
		{{767, 0, 0}, {767, 28, 386}, {0, 0, 764}, {0, 320, 387}, {767, 768, 1}, {767, 774, 765}},
		{{0, 0, 761}, {106, 0, 508}, {762, 769, 764}, {0, 767, 1}, {706, 697, 1}, {765, 179, 1}},
		{{0,0,900}, {200,200,816}, {800,900,800},	{900,400,800},	{900,500,202}, {800,200,0}},
		{{0,0,766}, {766,150,0}, {0,50,766}, {766,100,0}, {0,150,766}, {766,50,0}},
		{{0,0,1000}, {0,100,766}, {0,200,666}, {0,300,500}, {0,350,500}, {0,350,400}},

		{{100,100,100},{100,100,100},{100,100,100},{100,100,100},{100,100,100},{100,100,100}},
		{{100,100,100},{100,100,100},{100,100,100},{100,100,100},{100,100,100},{100,100,100}},
		{{100,100,100},{100,100,100},{100,100,100},{100,100,100},{100,100,100},{100,100,100}},
		{{100,100,100},{100,100,100},{100,100,100},{100,100,100},{100,100,100},{100,100,100}},
		{{100,100,100},{100,100,100},{100,100,100},{100,100,100},{100,100,100},{100,100,100}},
		{{100,100,100},{100,100,100},{100,100,100},{100,100,100},{100,100,100},{100,100,100}},
		{{100,100,100},{100,100,100},{100,100,100},{100,100,100},{100,100,100},{100,100,100}},
		{{100,100,100},{100,100,100},{100,100,100},{100,100,100},{100,100,100},{100,100,100}},
		{{100,100,100},{100,100,100},{100,100,100},{100,100,100},{100,100,100},{100,100,100}}

};

const float SCALE_BANK_COLOR[NUMSCALEBANKS][3]={

		{1000,1000,1000}, 	//white: western
		{1000,0,0},		//red: indian
		{0,0,1000},		//blue: alpha sp2
		{0,800,800},		//cyan: alpha sp1
		{0,1000,0},		//green: gamma sp1
		{1000,1000,0},		//yellow: 17ET
		{800,0,800},		//pink: twelve tone
		{700,0,50}		//orange: diatonic1

};

void calculate_envout_leds(uint16_t env_out_leds[NUM_CHANNELS][3]){
	uint8_t chan=0;

	if (select_colors_mode || assign_color_mode){
		for (chan=0;chan<6;chan++){
			env_out_leds[chan][0]=(uint16_t)((COLOR_CH[cur_colsch][chan][0])  );
			env_out_leds[chan][1]=(uint16_t)((COLOR_CH[cur_colsch][chan][1])  );
			env_out_leds[chan][2]=(uint16_t)((COLOR_CH[cur_colsch][chan][2])  );
		}
	} else {
		for (chan=0;chan<6;chan++){
			env_out_leds[chan][0]=(uint16_t)((COLOR_CH[cur_colsch][chan][0]) * ( (float)ENVOUT_PWM[chan]/4096.0) );
			env_out_leds[chan][1]=(uint16_t)((COLOR_CH[cur_colsch][chan][1]) * ( (float)ENVOUT_PWM[chan]/4096.0) );
			env_out_leds[chan][2]=(uint16_t)((COLOR_CH[cur_colsch][chan][2]) * ( (float)ENVOUT_PWM[chan]/4096.0) );
		}
	}

}

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
//DEBUGA_ON(DEBUG3);

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
	if (assign_color_mode) flash=1;
	else flash=1-flash;

	for (chan=0;chan<6;chan++){
		if (assign_color_mode) t_f=1.0;
		else
			t_f = channel_level[chan] < 0.05 ? 0.05 : channel_level[chan];

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
			next_i=motion_fadeto_note[chan];
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
//	DEBUGA_OFF(DEBUG3);

	calculate_envout_leds(env_out_leds);
#endif

	LEDDriver_set_LED_ring(ring, env_out_leds);
}



void display_scale(void){
	uint16_t ring[20][3];
	uint8_t i,j, chan;
	static uint8_t flash=0;
	uint16_t env_out_leds[6][3];
	float inv_fade[6],fade[6];

	uint8_t elacs[NUMSCALES][NUM_CHANNELS];
	uint8_t elacs_num[NUMSCALES];
	static uint8_t elacs_ctr[NUMSCALES]={0,0,0,0,0,0,0,0,0,0,0};

	//slow down the flashing
	if (flash++>3) flash=0;

	//Destination of fade:

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
		elacs[motion_scale_dest[i]][elacs_num[motion_scale_dest[i]]] = i;
		elacs_num[motion_scale_dest[i]]++;
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

	calculate_envout_leds(env_out_leds);

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

	calculate_envout_leds(env_out_leds);

	LEDDriver_set_LED_ring(ring, env_out_leds);

}


inline void update_LED_ring(int16_t change_scale_mode){

	static uint32_t led_ring_update_ctr=0;

	if (led_ring_update_ctr++>2000 || flag_update_LED_ring){
		led_ring_update_ctr=0;
		flag_update_LED_ring=0;

		if (change_scale_mode){
			display_scale();
		} else {
			//if (display_spec) display_spectral_readout();
			//else
			display_filter_rotation();
		}

	}


}



uint8_t old_lock[NUM_CHANNELS];

void enter_assign_color_mode(void){
	uint8_t i;


	for (i=0;i<NUM_CHANNELS;i++){
		already_handled_lock_release[i]=1;
		lock_pressed[i]=0;
		lock_up[i]=0;
		old_lock[i]=lock[i];
		lock[i]=1;
	}
	assign_color_mode=1;

}
void exit_assign_color_mode(void){
	uint8_t i;

	for (i=0;i<NUM_CHANNELS;i++){
		lock[i]=old_lock[i];
	}
	write_assigned_colors();
	assign_color_mode=0;
}

void exit_select_colors_mode(void){
	uint8_t i, blank_channels=0;

	select_colors_mode=0;

	//see if this colscheme is blank
	blank_channels=0;
	for (i=0;i<NUM_CHANNELS;i++){
		if (COLOR_CH[cur_colsch][i][0]==100 && COLOR_CH[cur_colsch][i][1]==100 && COLOR_CH[cur_colsch][i][2]==100)
			blank_channels++;
	}
	if (blank_channels==6) cur_colsch=0;
}


void do_assign_colors(void){
	uint8_t i, color_assign;
	color_assign=0xFF;

	for (i=0;i<6;i++) {if (lock[i]==0) {color_assign=i;i=6;}}

	if (color_assign!=0xFF){
		COLOR_CH[cur_colsch][color_assign][0] = potadc_buffer[0+SLIDER_ADC_BASE]>>2;
		COLOR_CH[cur_colsch][color_assign][1] = potadc_buffer[1+SLIDER_ADC_BASE]>>2;
		COLOR_CH[cur_colsch][color_assign][2] = potadc_buffer[2+SLIDER_ADC_BASE]>>2;
	}
}

void read_assigned_colors(void){
	uint32_t sz;

	cur_colsch = flash_read_word(FLASHADDR_colorscheme) - 0xAA;

	if (cur_colsch>=0 && cur_colsch<16) {
		flash_read_array((uint32_t *)COLOR_CH, FLASHADDR_colorscheme + 4, &sz);

		//u_NUM_COLORSCHEMES = sz / (NUM_CHANNELS * sizeof(uint32_t) * 3);
	} else {
		cur_colsch=0;
	}
}

void write_assigned_colors(void){
	uint32_t sz;
	uint32_t address;

	FLASH_Status status;

	sz=u_NUM_COLORSCHEMES * NUM_CHANNELS *  sizeof(uint32_t) * 3;
	//sz=sizeof(COLOR_CH);

	address=FLASHADDR_colorscheme;

	flash_erase_sector(address);

	flash_program_word(cur_colsch + 0xAA, address);

	flash_program_array((uint32_t *)COLOR_CH, address+4, sz);

}


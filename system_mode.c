/*
 * system_mode.c - System Mode for saving parameters into slots
 * --also handles editting color schemes and other global system parameters
 *
 * Author: Dan Green (danngreen1@gmail.com)
 * 2015
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
#include "system_mode.h"
#include "dig_inouts.h"

extern float exp_1voct[4096];

enum UI_Modes ui_mode=PLAY;

uint8_t cur_param_bank=0xFF;


extern __IO uint16_t potadc_buffer[NUM_ADCS];
extern __IO uint16_t adc_buffer[NUM_ADCS];

extern enum Filter_Types filter_type;
extern enum Filter_Modes filter_mode;
extern uint32_t freqblock;
extern int num_freq_blocked;

extern uint8_t cur_colsch;
extern float COLOR_CH[16][6][3];

extern uint8_t lock[NUM_CHANNELS];
extern uint8_t q_locked[NUM_CHANNELS];
extern uint32_t qval[NUM_CHANNELS];
extern float freq_nudge[NUM_CHANNELS];

extern uint8_t note[NUM_CHANNELS];
extern uint8_t scale[NUM_CHANNELS];
extern uint8_t scale_bank[NUM_CHANNELS];

extern float rot_dir[6];
extern int8_t motion_fadeto_note[NUM_CHANNELS];
extern int8_t motion_fadeto_scale[NUM_CHANNELS];
extern int8_t motion_spread_dest[NUM_CHANNELS];
extern int8_t motion_spread_dir[NUM_CHANNELS];
extern int8_t motion_scale_dest[NUM_CHANNELS];
extern uint8_t hover_scale_bank;

extern uint8_t slider_led_mode;

extern float trackcomp[2];
extern int16_t trackoffset[2];

extern float voltoct_pwm_tracking;

extern float user_scalebank[231];

#define FLASH_ADDR_userparams 0x08004000
//Globals:
#define FLASH_ADDR_firmware_version					FLASH_ADDR_userparams		/* 0  ..3	*/
#define SZ_FV 4

#define FLASH_ADDR_startupbank	(FLASH_ADDR_firmware_version	+ SZ_FV)		/* 4 		*/
#define SZ_SB 1

#define FLASH_ADDR_clipmode 		(FLASH_ADDR_startupbank 	+ SZ_SB) 		/* 5 		*/
#define SZ_CM 1

#define FLASH_ADDR_padding_1		(FLASH_ADDR_clipmode		+ SZ_CM)		/* 6  ..7	*/
#define SZ_P1 2

#define FLASH_ADDR_trackcomp1 		(FLASH_ADDR_padding_1 		+ SZ_P1) 		/* 8  ..11	*/
#define SZ_TC1 4

#define FLASH_ADDR_trackcomp2 		(FLASH_ADDR_trackcomp1 		+ SZ_TC1) 		/* 12 ..15	*/
#define SZ_TC2 4

#define FLASH_ADDR_trackoffset1 	(FLASH_ADDR_trackcomp2 		+ SZ_TC2) 		/* 16 ..19	*/
#define SZ_TO1 4

#define FLASH_ADDR_trackoffset2 	(FLASH_ADDR_trackoffset1 	+ SZ_TO1) 		/* 20 ..23	*/
#define SZ_TO2 4

#define FLASH_ADDR_voltoct_pwm_tracking (FLASH_ADDR_trackoffset2 + SZ_TO2) 		/* 24 ..27	*/
#define SZ_VOT 4

#define FLASH_ADDR_future_globals	(FLASH_ADDR_voltoct_pwm_tracking + SZ_VOT)		/* 28 ..133 */
#define SZ_FG 106

//Parameter banks:
#define FLASH_ADDR_START_PARAMBANKS		(FLASH_ADDR_future_globals	+ SZ_FG)		/* 134 		*/

#define FLASH_OFFSET_bankstatus		(0)											/* +0 		 */
#define FLASH_OFFSET_note			(FLASH_OFFSET_bankstatus		+ 1)		/* +1..6 	 */
#define FLASH_OFFSET_scale			(FLASH_OFFSET_note				+ 6)		/* +7..12 	 */
#define FLASH_OFFSET_scale_bank		(FLASH_OFFSET_scale				+ 6)		/* +13..18 	 */
#define FLASH_OFFSET_lock			(FLASH_OFFSET_scale_bank		+ 6)		/* +19..24 	 */
#define FLASH_OFFSET_q_locked		(FLASH_OFFSET_lock				+ 6)		/* +25..30 	 */
#define FLASH_OFFSET_qval			(FLASH_OFFSET_q_locked			+ 6)		/* +31..54 	 */
#define FLASH_OFFSET_filter_type	(FLASH_OFFSET_qval				+ 24)		/* +55 		 */
#define FLASH_OFFSET_cur_colsch		(FLASH_OFFSET_filter_type		+ 1)		/* +56 		 */
#define FLASH_OFFSET_freq_nudge		(FLASH_OFFSET_cur_colsch		+ 1)		/* +57..+80  */
#define FLASH_OFFSET_filter_mode	(FLASH_OFFSET_freq_nudge		+ 24)		/* 	81 	 */
#define FLASH_OFFSET_freqblock		(FLASH_OFFSET_filter_mode		+ 1)		/* 	82 .. 85	 */
#define FLASH_OFFSET_future_params	(FLASH_OFFSET_freqblock			+ 4)		/*  86 ... */

#define SZ_FP 74

#define FLASH_SIZE_parambank	(FLASH_OFFSET_future_params		+ SZ_FP)		/* size of each param bank: 160 */
#define FLASH_NUM_parambanks 6													/* size of all param banks: 960 */

//#define FLASH_ADDR_END_PARAMBANKS (FLASH_SIZE_parambank*FLASH_NUM_parambanks + FLASH_ADDR_START_PARAMBANKS) /* 1094 */
//#define FLASH_freespace_after_parambanks 953									/* unused space: 1095 - 2047 */

#define FLASH_ADDR_colschem 		FLASH_ADDR_userparams + 2048				/* 2048			*/
#define FLASH_SIZE_colschem (NUM_COLORSCHEMES * NUM_CHANNELS *  4 * 3) 			/* size: 1152	*/
#define FLASH_END_colschem (FLASH_ADDR_colschem + FLASH_SIZE_colschem)			/* 3200			*/

//#define FLASH_freespace_after_colscheme 13183									/* unused space: 3211 - 15359 */

#define FLASH_ADDR_user_scalebank		FLASH_ADDR_userparams + 15360			/* 15360		*/
#define FLASH_SIZE_user_scalebank		(NUMSCALES * (NUM_FILTS + 1) * 4)		/* size: 924 	*/

#define FLASH_SYMBOL_bankfilled 0x01
#define FLASH_SYMBOL_startupoffset 0xAA
#define FLASH_SYMBOL_firmwareoffset 0xAA550000

uint32_t 	flash_firmware_version=0;
uint8_t		flash_startupbank=0;
uint8_t 	flash_clipmode=0;
float 		flash_trackcomp[2]={1.0,1.0};
int32_t 	flash_trackoffset[2]={0,0};
uint8_t 	flash_bankstatus[FLASH_NUM_parambanks];
uint8_t 	flash_note[FLASH_NUM_parambanks][NUM_CHANNELS];
uint8_t 	flash_scale[FLASH_NUM_parambanks][NUM_CHANNELS];
uint8_t 	flash_scale_bank[FLASH_NUM_parambanks][NUM_CHANNELS];
uint8_t 	flash_lock[FLASH_NUM_parambanks][NUM_CHANNELS];
uint8_t 	flash_q_locked[FLASH_NUM_parambanks][NUM_CHANNELS];
uint32_t 	flash_qval[FLASH_NUM_parambanks][NUM_CHANNELS];
float 		flash_freq_nudge[FLASH_NUM_parambanks][NUM_CHANNELS];
uint8_t 	flash_filter_type[FLASH_NUM_parambanks];
uint8_t 	flash_filter_mode[FLASH_NUM_parambanks];
uint32_t 	flash_freqblock[FLASH_NUM_parambanks];
uint8_t 	flash_cur_colsch[FLASH_NUM_parambanks];
float 		flash_COLOR_CH[16][6][3];
float 		flash_user_scalebank[231];
float 		flash_voltoct_pwm_tracking;


void factory_reset(void){
	uint8_t i,j;


	flash_firmware_version = FW_VERSION;
	flash_startupbank=0;
	flash_clipmode=SHOW_CLIPPING;
	flash_trackcomp[0]=1.0;
	flash_trackcomp[1]=1.0;
	flash_trackoffset[0]=0;
	flash_trackoffset[1]=0;
	flash_voltoct_pwm_tracking = 1.0;

	for (i=0;i<FLASH_NUM_parambanks;i++){
		flash_bankstatus[i] 		= 0xFF;
		flash_filter_type[i] 		= MAXQ;
		flash_filter_mode[i] 		= TWOPASS;
		flash_freqblock[i] 			= 0b00000000000000000000;
		flash_cur_colsch[i]			= 0;

		for (j=0;j<NUM_CHANNELS;j++){
			flash_note[i][j]		= j+5;
			flash_scale[i][j]		= 6;
			flash_scale_bank[i][j]  = 0;
			flash_lock[i][j]		= 0;
			flash_q_locked[i][j]	= 0;
			flash_qval[i][j]		= 0;
			flash_freq_nudge[i][j]	= 0;
		}

	}

	set_default_color_scheme(); //copies DEFAULT_COLOR_CH into COLOR_CH
	cur_colsch = 0;

	//copy COLOR_SCH into flash_COLOR_CH
	uint32_t sz=FLASH_SIZE_colschem;
	uint8_t *src;
	uint8_t *dst;
	src = (uint8_t *)COLOR_CH;
	dst = (uint8_t *)flash_COLOR_CH;
	while (sz--) *dst++ = *src++;

	set_default_user_scalebank();

	sz=FLASH_SIZE_user_scalebank;
	src = (uint8_t *)user_scalebank;
	dst = (uint8_t *)flash_user_scalebank;
	while (sz--) *dst++ = *src++;

	set_default_param_values();

	store_params_into_sram(0);

	write_all_params_to_FLASH();

}


//Loads the active parameters from an SRAM bank
void load_params_from_sram(uint8_t bank_num){
	uint8_t i,j;
	uint8_t already_set_hover=0;

	cur_colsch 			= flash_cur_colsch[bank_num];
	change_filter_type(	  flash_filter_type[bank_num]);
	filter_mode 		= flash_filter_mode[bank_num];
	freqblock 			= flash_freqblock[bank_num];
	num_freq_blocked	= 0;
	for (j=0; j<NUMSCALEBANKS; j++){
		if (freqblock & (1<<j))
		num_freq_blocked+=1;
	}	
	if (num_freq_blocked > 14){
	 freqblock = 0; //invalid
	 num_freq_blocked=0;
	 }
	
	for (i=0;i<NUM_CHANNELS;i++){
		note[i]			= flash_note[bank_num][i];
		scale[i]		= flash_scale[bank_num][i];
		scale_bank[i]	= flash_scale_bank[bank_num][i];

		lock[i]			= flash_lock[bank_num][i];
		q_locked[i]		= flash_q_locked[bank_num][i];
		qval[i]			= flash_qval[bank_num][i];
		freq_nudge[i]	= flash_freq_nudge[bank_num][i];

		rot_dir[i]				= 0;
		motion_fadeto_scale[i]	= scale[i];
		motion_scale_dest[i]	= scale[i];
		motion_spread_dir[i]	= 0;
		motion_spread_dest[i]	= note[i];
		motion_fadeto_note[i]	= note[i];

		if (!lock[i] && !already_set_hover) {
			hover_scale_bank = scale_bank[i];
			already_set_hover=1;
		}

	}

	if (!already_set_hover) hover_scale_bank = scale_bank[0];

}

//Stores the active parameters into an SRAM bank
void store_params_into_sram(uint8_t bank_num){
	uint8_t i;

	flash_cur_colsch[bank_num]			= cur_colsch;
	flash_filter_type[bank_num]			= filter_type;
	flash_filter_mode[bank_num]			= filter_mode;
	flash_freqblock[bank_num]			= freqblock;

	for (i=0;i<NUM_CHANNELS;i++){
		flash_note[bank_num][i]			= note[i];
		flash_scale[bank_num][i]		= scale[i];
		flash_scale_bank[bank_num][i]	= scale_bank[i];

		flash_lock[bank_num][i]			= lock[i];
		flash_q_locked[bank_num][i]		= q_locked[i];
		flash_qval[bank_num][i]			= qval[i];
		flash_freq_nudge[bank_num][i]	= freq_nudge[i];
	}
}

//load SRAM globals into active params
void load_global_params(void){
	uint8_t i;
	uint32_t sz;
	uint8_t *src;
	uint8_t *dst;

	sz=FLASH_SIZE_colschem;
	src = (uint8_t *)flash_COLOR_CH;
	dst = (uint8_t *)COLOR_CH;
	while (sz--) *dst++ = *src++;

	sz=FLASH_SIZE_user_scalebank;
	src = (uint8_t *)flash_user_scalebank;
	dst = (uint8_t *)user_scalebank;
	while (sz--) *dst++ = *src++;

	cur_colsch = flash_cur_colsch[flash_startupbank];
	if (cur_colsch<0 || cur_colsch>NUM_COLORSCHEMES) cur_colsch=0;

	slider_led_mode = (flash_clipmode==DONT_SHOW_CLIPPING) ? DONT_SHOW_CLIPPING : SHOW_CLIPPING;

	trackcomp[0] = flash_trackcomp[0];
	if (trackcomp[0]<0.5 || trackcomp[0]>2.0) trackcomp[0]=1.0; //sanity check

	trackoffset[0] = flash_trackoffset[0];
	if (trackoffset[0] < -100 || trackoffset[0] > 100) trackoffset[0]=0; //sanity check

	trackcomp[1] = flash_trackcomp[1];
	if (trackcomp[1]<0.5 || trackcomp[1]>2.0) trackcomp[1]=1.0; //sanity check

	trackoffset[1] = flash_trackoffset[1];
	if (trackoffset[1] < -100 || trackoffset[1] > 100) trackoffset[1]=0; //sanity check

	voltoct_pwm_tracking = flash_voltoct_pwm_tracking;
	if (voltoct_pwm_tracking < 0.5 || voltoct_pwm_tracking > 2.0) voltoct_pwm_tracking=1.0; //sanity check

}

//stores active global params into SRAM
void store_globals_into_sram(void){

	uint32_t sz;
	uint8_t *src;
	uint8_t *dst;

	sz=FLASH_SIZE_user_scalebank;
	src = (uint8_t *)user_scalebank;
	dst = (uint8_t *)flash_user_scalebank;
	while (sz--) *dst++ = *src++;


	sz=FLASH_SIZE_colschem;
	src = (uint8_t *)COLOR_CH;
	dst = (uint8_t *)flash_COLOR_CH;
	while (sz--) *dst++ = *src++;

	if (flash_startupbank>=6) flash_startupbank=0; //this indicates that FLASH user memory has been written to, thus we can safely read global params

	flash_cur_colsch[flash_startupbank]	= cur_colsch;

	flash_clipmode = (slider_led_mode==DONT_SHOW_CLIPPING) ? DONT_SHOW_CLIPPING : SHOW_CLIPPING;

	flash_trackcomp[0] = trackcomp[0];
	flash_trackoffset[0] = trackoffset[0];
	flash_trackcomp[1] = trackcomp[1];
	flash_trackoffset[1] = trackoffset[1];

	flash_voltoct_pwm_tracking =  voltoct_pwm_tracking;
}



//Loads globals from flash. Checks the startupbank value in FLASH. If it's programmed, then load the parameters from that bank.
uint8_t load_startup_params(void){

	flash_firmware_version = flash_read_word(FLASH_ADDR_firmware_version) - FLASH_SYMBOL_firmwareoffset;

	if (flash_firmware_version > 0 && flash_firmware_version < 500){

		read_all_params_from_FLASH();
		load_global_params();

		if (flash_startupbank >= 0 && flash_startupbank < 6){

			if (is_bank_filled(flash_startupbank)){

				load_params_from_sram(flash_startupbank);

			} else
				flash_startupbank=0;
		}else
			flash_startupbank=0;
	} else
		flash_startupbank=0xFF;

	return(flash_startupbank);
}

//Reads from FLASH memory and stores it in SRAM variables
void read_all_params_from_FLASH(void){ //~200uS
	uint8_t bank_i;
	uint32_t t;

	flash_firmware_version = flash_read_word(FLASH_ADDR_firmware_version) - FLASH_SYMBOL_firmwareoffset;

	flash_startupbank = flash_read_byte(FLASH_ADDR_startupbank) - FLASH_SYMBOL_startupoffset;

	flash_clipmode = flash_read_byte(FLASH_ADDR_clipmode);

	t = flash_read_word(FLASH_ADDR_trackcomp1);
	if (t==0xFFFFFFFF) t=1;
	flash_trackcomp[0]=*(float *)&t;

	t = flash_read_word(FLASH_ADDR_trackcomp2);
	if (t==0xFFFFFFFF) t=1;
	flash_trackcomp[1]=*(float *)&t;

	flash_trackoffset[0] = flash_read_word(FLASH_ADDR_trackoffset1);
	flash_trackoffset[1] = flash_read_word(FLASH_ADDR_trackoffset2);


	t = flash_read_word(FLASH_ADDR_voltoct_pwm_tracking);
	if (t==0xFFFFFFFF) t=1;
	flash_voltoct_pwm_tracking=*(float *)&t;

	for (bank_i=0;bank_i<6;bank_i++){
		flash_bankstatus[bank_i] = flash_read_byte(FLASH_ADDR_START_PARAMBANKS + FLASH_OFFSET_bankstatus + (FLASH_SIZE_parambank * bank_i));

		flash_read_array((uint8_t *)flash_note[bank_i], FLASH_ADDR_START_PARAMBANKS + FLASH_OFFSET_note + (FLASH_SIZE_parambank * bank_i), 6);
		flash_read_array((uint8_t *)flash_scale[bank_i], FLASH_ADDR_START_PARAMBANKS + FLASH_OFFSET_scale + (FLASH_SIZE_parambank * bank_i), 6);
		flash_read_array((uint8_t *)flash_scale_bank[bank_i], FLASH_ADDR_START_PARAMBANKS + FLASH_OFFSET_scale_bank + (FLASH_SIZE_parambank * bank_i), 6);

		flash_read_array((uint8_t *)flash_lock[bank_i], FLASH_ADDR_START_PARAMBANKS + FLASH_OFFSET_lock + (FLASH_SIZE_parambank * bank_i), 6);
		flash_read_array((uint8_t *)flash_q_locked[bank_i], FLASH_ADDR_START_PARAMBANKS + FLASH_OFFSET_q_locked + (FLASH_SIZE_parambank * bank_i), 6);
		flash_read_array((uint8_t *)flash_qval[bank_i], FLASH_ADDR_START_PARAMBANKS + FLASH_OFFSET_qval + (FLASH_SIZE_parambank * bank_i), 24);

		flash_filter_type[bank_i] = flash_read_byte(FLASH_ADDR_START_PARAMBANKS + FLASH_OFFSET_filter_type + (FLASH_SIZE_parambank * bank_i));
		flash_filter_mode[bank_i] = flash_read_byte(FLASH_ADDR_START_PARAMBANKS + FLASH_OFFSET_filter_mode + (FLASH_SIZE_parambank * bank_i));

		//Force ONEPASS if we read a legacy slot as BPRE
		if (flash_filter_type[bank_i] == BPRE) flash_filter_mode[bank_i] = ONEPASS;

		//Force TWOPASS for legacy banks
		if (flash_filter_mode[bank_i] != ONEPASS) flash_filter_mode[bank_i] = TWOPASS;


		flash_cur_colsch[bank_i]  = flash_read_byte(FLASH_ADDR_START_PARAMBANKS + FLASH_OFFSET_cur_colsch  + (FLASH_SIZE_parambank * bank_i));
		flash_freqblock[bank_i]   = flash_read_word(FLASH_ADDR_START_PARAMBANKS + FLASH_OFFSET_freqblock  + (FLASH_SIZE_parambank * bank_i));

		flash_read_array((uint8_t  *)flash_freq_nudge[bank_i], FLASH_ADDR_START_PARAMBANKS + FLASH_OFFSET_freq_nudge + (FLASH_SIZE_parambank * bank_i), 24);

	}

	flash_read_array((uint8_t *)flash_COLOR_CH, FLASH_ADDR_colschem, FLASH_SIZE_colschem);

	flash_read_array((uint8_t *)flash_user_scalebank, FLASH_ADDR_user_scalebank, FLASH_SIZE_user_scalebank);

}

void read_one_bank_params_from_FLASH(uint8_t bank_i){

	flash_bankstatus[bank_i] = flash_read_byte(FLASH_ADDR_START_PARAMBANKS + FLASH_OFFSET_bankstatus + (FLASH_SIZE_parambank * bank_i));

	flash_read_array((uint8_t *)flash_note[bank_i], FLASH_ADDR_START_PARAMBANKS + FLASH_OFFSET_note + (FLASH_SIZE_parambank * bank_i), 6);
	flash_read_array((uint8_t *)flash_scale[bank_i], FLASH_ADDR_START_PARAMBANKS + FLASH_OFFSET_scale + (FLASH_SIZE_parambank * bank_i), 6);
	flash_read_array((uint8_t *)flash_scale_bank[bank_i], FLASH_ADDR_START_PARAMBANKS + FLASH_OFFSET_scale_bank + (FLASH_SIZE_parambank * bank_i), 6);

	flash_read_array((uint8_t  *)flash_lock[bank_i], FLASH_ADDR_START_PARAMBANKS + FLASH_OFFSET_lock + (FLASH_SIZE_parambank * bank_i), 6);
	flash_read_array((uint8_t  *)flash_q_locked[bank_i], FLASH_ADDR_START_PARAMBANKS + FLASH_OFFSET_q_locked + (FLASH_SIZE_parambank * bank_i), 6);
	flash_read_array((uint8_t  *)flash_qval[bank_i], FLASH_ADDR_START_PARAMBANKS + FLASH_OFFSET_qval + (FLASH_SIZE_parambank * bank_i), 24);
	flash_read_array((uint8_t  *)flash_freq_nudge[bank_i], FLASH_ADDR_START_PARAMBANKS + FLASH_OFFSET_freq_nudge + (FLASH_SIZE_parambank * bank_i), 24);

	flash_freqblock[bank_i]   = flash_read_word(FLASH_ADDR_START_PARAMBANKS + FLASH_OFFSET_freqblock + (FLASH_SIZE_parambank * bank_i));
	flash_filter_type[bank_i] = flash_read_byte(FLASH_ADDR_START_PARAMBANKS + FLASH_OFFSET_filter_type + (FLASH_SIZE_parambank * bank_i));
	flash_filter_mode[bank_i] = flash_read_byte(FLASH_ADDR_START_PARAMBANKS + FLASH_OFFSET_filter_mode + (FLASH_SIZE_parambank * bank_i));

	flash_cur_colsch[bank_i] = flash_read_byte(FLASH_ADDR_START_PARAMBANKS + FLASH_OFFSET_cur_colsch + (FLASH_SIZE_parambank * bank_i));


}

//Writes the SRAM variables into FLASH memory
void write_all_params_to_FLASH(void){
	uint8_t bank_i;

	flash_begin_open_program();

	flash_open_erase_sector(FLASH_ADDR_userparams);

	flash_open_program_word(flash_firmware_version + FLASH_SYMBOL_firmwareoffset, FLASH_ADDR_firmware_version);

	flash_open_program_byte(flash_startupbank + FLASH_SYMBOL_startupoffset, FLASH_ADDR_startupbank);

	flash_open_program_byte(flash_clipmode, FLASH_ADDR_clipmode);

	for (bank_i=0;bank_i<6;bank_i++){
		flash_open_program_byte(flash_bankstatus[bank_i], FLASH_ADDR_START_PARAMBANKS + FLASH_OFFSET_bankstatus + (FLASH_SIZE_parambank * bank_i));

		flash_open_program_array((uint8_t *)flash_note[bank_i], FLASH_ADDR_START_PARAMBANKS + FLASH_OFFSET_note + (FLASH_SIZE_parambank * bank_i), 6);
		flash_open_program_array((uint8_t *)flash_scale[bank_i], FLASH_ADDR_START_PARAMBANKS + FLASH_OFFSET_scale + (FLASH_SIZE_parambank * bank_i), 6);
		flash_open_program_array((uint8_t *)flash_scale_bank[bank_i], FLASH_ADDR_START_PARAMBANKS + FLASH_OFFSET_scale_bank + (FLASH_SIZE_parambank * bank_i), 6);

		flash_open_program_array((uint8_t *)flash_lock[bank_i], FLASH_ADDR_START_PARAMBANKS + FLASH_OFFSET_lock + (FLASH_SIZE_parambank * bank_i), 6);
		flash_open_program_array((uint8_t *)flash_q_locked[bank_i], FLASH_ADDR_START_PARAMBANKS + FLASH_OFFSET_q_locked + (FLASH_SIZE_parambank * bank_i), 6);
		flash_open_program_array((uint8_t *)flash_qval[bank_i], FLASH_ADDR_START_PARAMBANKS + FLASH_OFFSET_qval + (FLASH_SIZE_parambank * bank_i), 24);

		flash_open_program_byte(flash_filter_type[bank_i], FLASH_ADDR_START_PARAMBANKS + FLASH_OFFSET_filter_type + (FLASH_SIZE_parambank * bank_i));
		flash_open_program_byte(flash_filter_mode[bank_i], FLASH_ADDR_START_PARAMBANKS + FLASH_OFFSET_filter_mode + (FLASH_SIZE_parambank * bank_i));
		flash_open_program_byte(flash_cur_colsch[bank_i],  FLASH_ADDR_START_PARAMBANKS + FLASH_OFFSET_cur_colsch  + (FLASH_SIZE_parambank * bank_i));
		
		flash_open_program_word(flash_freqblock[bank_i],  FLASH_ADDR_START_PARAMBANKS + FLASH_OFFSET_freqblock  + (FLASH_SIZE_parambank * bank_i));

		flash_open_program_array((uint8_t  *)flash_freq_nudge[bank_i], FLASH_ADDR_START_PARAMBANKS + FLASH_OFFSET_freq_nudge + (FLASH_SIZE_parambank * bank_i), 24);

	}

	flash_open_program_array((uint8_t *)flash_COLOR_CH, FLASH_ADDR_colschem, FLASH_SIZE_colschem);

	flash_open_program_array((uint8_t *)flash_user_scalebank, FLASH_ADDR_user_scalebank, FLASH_SIZE_user_scalebank);

	flash_open_program_word(*(uint32_t *)&(flash_trackoffset[0]), FLASH_ADDR_trackoffset1);
	flash_open_program_word(*(uint32_t *)&(flash_trackoffset[1]), FLASH_ADDR_trackoffset2);

	flash_open_program_word(*(uint32_t *)&(flash_trackcomp[0]), FLASH_ADDR_trackcomp1);
	flash_open_program_word(*(uint32_t *)&(flash_trackcomp[1]), FLASH_ADDR_trackcomp2);

	flash_open_program_word(*(uint32_t *)&(flash_voltoct_pwm_tracking), FLASH_ADDR_voltoct_pwm_tracking);


	flash_end_open_program();
}



void save_param_bank(uint8_t bank_num){

	read_all_params_from_FLASH();

	if (bank_num>=0 && bank_num<6){
		//Set the startup bank to the most recently saved bank (this one)
		flash_startupbank = bank_num;

		flash_bankstatus[bank_num] = FLASH_SYMBOL_bankfilled;

		store_params_into_sram(bank_num);
	}

	store_globals_into_sram();

	write_all_params_to_FLASH();

	cur_param_bank = bank_num;
}

void load_param_bank(uint8_t bank_num){

	read_one_bank_params_from_FLASH(bank_num);

	//Make sure this bank is filled... and then load the SRAM variables into the active variables
	if (is_bank_filled(bank_num)){

		load_params_from_sram(bank_num);

		cur_param_bank = bank_num;

		//Set the startup bank to the most recently loaded bank (this one)
		//this won't get saved in flash until we write to flash by saving the color scheme or user scalebank
		flash_startupbank = bank_num;

	}


}

uint8_t is_bank_filled(uint8_t bank_num){

	if (flash_bankstatus[bank_num] == FLASH_SYMBOL_bankfilled) return(1);
	else return(0);

}


void exit_select_colors_mode(void){
	uint8_t i, blank_channels=0;

	//see if this colscheme is blank
	blank_channels=0;
	for (i=0;i<NUM_CHANNELS;i++){
		if (COLOR_CH[cur_colsch][i][0]==100 && COLOR_CH[cur_colsch][i][1]==100 && COLOR_CH[cur_colsch][i][2]==100)
			blank_channels++;
	}
	if (blank_channels>0) cur_colsch=0;
}


uint8_t old_lock[NUM_CHANNELS];

void enter_system_mode(void){
	uint8_t i;

	ui_mode=SELECT_PARAMS;

	for (i=0;i<NUM_CHANNELS;i++){
		old_lock[i]=lock[i];
	}

}


void exit_system_mode(uint8_t reset_locks){
	uint8_t i;

	store_globals_into_sram(); //why?

	if (reset_locks==1){
		for (i=0;i<NUM_CHANNELS;i++){
			lock[i]=old_lock[i];
		}
	}

	ui_mode=PLAY;
}

void enter_edit_color_mode(void){
	uint8_t i;

	ui_mode=EDIT_COLORS;

	for (i=0;i<NUM_CHANNELS;i++){

		//already_handled_lock_release[i]=1;
		//lock_pressed[i]=0;
		//lock_up[i]=0;

		lock[i]=1;
	}

}

void handle_edit_colors(void){
	uint8_t i;

	for (i=0;i<6;i++) {
		if (lock[i]==0){

			COLOR_CH[cur_colsch][i][0] = (uint16_t)(exp_1voct[potadc_buffer[0+SLIDER_ADC_BASE]]);
			COLOR_CH[cur_colsch][i][1] = (uint16_t)(exp_1voct[potadc_buffer[1+SLIDER_ADC_BASE]]);
			COLOR_CH[cur_colsch][i][2] = (uint16_t)(exp_1voct[potadc_buffer[2+SLIDER_ADC_BASE]]);

		}
	}

}

void handle_lock_switch(void){
	static uint16_t state=0;
	static uint16_t ctr=0;
	static uint16_t switch_state=0;
	uint16_t t;

	ctr++;
	if (ctr==80000){ //User took too long, start over
		state=0;
		ctr=0;
	}

	if (state==4){

		//do something

		state=0;
		ctr=0;
	}

	if (MOD135) t=0xe000; else t=0xe001;
	switch_state=(switch_state<<1) | t;
	if ((state&1) && switch_state==0xff00){
		state++;
		ctr=0;
	}
	if (!(state&1) && switch_state==0xe00f){
		state++;
		ctr=0;
	}

}

void handle_freqpot_changing_filtermode(void){
	static uint16_t state=0;
	static uint16_t ctr=0;

	ctr++;
	if (ctr==18000){ //User took too long, start over
		state=0;
		ctr=0;
	}

	if (state==3){

		if (filter_type==MAXQ && filter_mode == ONEPASS)
			change_filter_type(BPRE);
		else
			change_filter_type(MAXQ);

		state=0;
		ctr=0;
	}

	if ((state&1) && adc_buffer[FREQNUDGE6_ADC] > 3500){
		state++;
		ctr=0;
	}
	if (!(state&1) && adc_buffer[FREQNUDGE6_ADC] < 500){
		state++;
		ctr=0;
	}
}

void handle_freqpot_changing_filtermode_mode(void){
	static uint16_t state=0;
	static uint16_t ctr=0;

	ctr++;
	if (ctr==18000){ //User took too long, start over
		state=0;
		ctr=0;
	}

	if (state==3){

		if (filter_mode==TWOPASS)
			filter_mode=ONEPASS;
		else
			filter_mode=TWOPASS;
			change_filter_type(MAXQ);

		state=0;
		ctr=0;
	}

	if ((state&1) && adc_buffer[FREQNUDGE1_ADC] > 3500){
		state++;
		ctr=0;
	}
	if (!(state&1) && adc_buffer[FREQNUDGE1_ADC] < 500){
		state++;
		ctr=0;
	}
}

void handle_slider_changing_clipmode(void){
	static uint16_t state=0;
	static uint16_t ctr=0;

	ctr++;
	if (ctr==10000){ //User took too long, start over
		state=0;
		ctr=0;
	}

	if (state==7){

		if (slider_led_mode==DONT_SHOW_CLIPPING)
			slider_led_mode=SHOW_CLIPPING;
		else
			slider_led_mode=DONT_SHOW_CLIPPING;

		state=0;
		ctr=0;
	}

	if ((state&1) && potadc_buffer[SLIDER_ADC_BASE] > 4000){
		state++;
		ctr=0;
	}
	if (!(state&1) && potadc_buffer[SLIDER_ADC_BASE] < 100){
		state++;
		ctr=0;
	}
}

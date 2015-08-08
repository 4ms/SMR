/*
 * system_mode.h
 *
 *  Created on: Jul 7, 2015
 *      Author: design
 */

#ifndef SYSTEM_MODE_H_
#define SYSTEM_MODE_H_

enum UI_Modes {
	PLAY,
	CHANGE_SCALE,
	PRE_SELECT_PARAMS,
	SELECT_PARAMS,
	PRE_EDIT_COLORS,
	EDIT_COLORS,
	EDIT_SCALES
};

void exit_select_colors_mode(void);
void enter_system_mode(void);
void exit_system_mode(uint8_t reset_locks);
//void exit_assign_colors_mode(void);

void handle_edit_colors(void);

void handle_freqpot_changing_filtermode(void);
void handle_slider_changing_clipmode(void);

void read_all_params_from_FLASH(void);
void write_all_params_to_FLASH(void);
uint8_t is_bank_filled(uint8_t bank_num);

void load_param_bank(uint8_t bank_num);
void save_param_bank(uint8_t bank_num);
uint8_t load_startupbank(void);

void store_params_into_sram(uint8_t bank_num);

#endif /* SYSTEM_MODE_H_ */
//SELECT_COLORS

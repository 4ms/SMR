/*
 * system_mode.h
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



#ifndef SYSTEM_MODE_H_
#define SYSTEM_MODE_H_

#include <stm32f4xx.h>

enum UI_Modes {
	PLAY,
	CHANGE_SCALE,
	PRE_SELECT_PARAMS,
	SELECT_PARAMS,
	PRE_EDIT_COLORS,
	EDIT_COLORS,
	EDIT_SCALES
};


void factory_reset(void);
void load_params_from_sram(uint8_t bank_num);
void store_params_into_sram(uint8_t bank_num);
void load_global_params(void);
void store_globals_into_sram(void);
uint8_t load_startup_params(void);
void read_all_params_from_FLASH(void);
void read_one_bank_params_from_FLASH(uint8_t bank_i);
void write_all_params_to_FLASH(void);
void save_param_bank(uint8_t bank_num);
void load_param_bank(uint8_t bank_num);
uint8_t is_bank_filled(uint8_t bank_num);

void exit_select_colors_mode(void);

void enter_system_mode(void);
void exit_system_mode(uint8_t reset_locks);

void enter_edit_color_mode(void);
void handle_edit_colors(void);

void handle_lock_switch(void);
void handle_freqpot_changing_filtermode(void);
void handle_freqpot_changing_filtermode_mode(void);
void handle_slider_changing_clipmode(void);


#endif /* SYSTEM_MODE_H_ */
//SELECT_COLORS

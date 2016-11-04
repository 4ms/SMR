/*
 * params.h
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

#include <stm32f4xx.h>

#ifndef PARAMS_H_
#define PARAMS_H_

//10s  is 0.999998958333873 or 1-0.00000104166613  @4096
//1s   is 0.999989583387583 or 1-0.00001041661242  @2048
//0.1s is 0.999895838758489 or 1-0.00010416124151  @1024
//10ms is 0.99895838758489  or 1-0.0010416124151  @512

#define FREQNUDGE_LPF	0.995
#define FREQCV_LPF	0.99
#define NUDGEPOT_MIN_CHANGE 0.0029304029304 // f_nudge_range / max pot val * num_pts_to_ignore <=> 1.2/4095*10 
#define ENABLE_M 1

#define BASE_TRACKOFFSET (-11)

#define ROTARY_BUTTON_HOLD 100000

#define DISPLAYTIME_NUDGEBOTHSIDES 25000
#define DISPLAYTIME_NUDGEONESIDE 12000

//#define LAG_ATTACK_MIN_LPF 0.90
//#define LAG_DECAY_MIN_LPF 0.95
#define CHANNEL_LEVEL_MIN_LPF 0.75

#define ADC_POT_LPF		0.99
#define ADC_JACK_LPF	0.90

#define QPOT_MIN_CHANGE 100
#define QPOT_LPF 0.95
#define QCV_LPF 0.95

#define QLOCK_FLASH_SPEED 1500

#define LOCK_BUTTON_QUNLOCK_HOLD_TIME 50000

#define LOCK_BUTTON_LONG_HOLD_TIME 80000

#define SPREAD_ADC_HYSTERESIS 75


void param_read_freq(void);
void param_read_channel_level(void);
// void param_read_one_channel_level(uint8_t i);
void param_poll_switches(void);
void param_read_q(void);
void param_read_one_q(int16_t i);
inline void update_lock_leds(void);
inline uint8_t num_locks_pressed(void);
void param_read_lock_buttons(void);
void init_freq_update_timer(void);

enum Env_Out_Modes{
ENV_SLOW,
ENV_FAST,
ENV_TRIG,
ENV_VOLTOCT

};

#endif /* PARAMS_H_ */

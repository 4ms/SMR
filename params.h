/*
 * params.h
 *
 *  Created on: Jun 17, 2015
 *      Author: design
 */
#include "stm32f4xx.h"

#ifndef PARAMS_H_
#define PARAMS_H_

//10s  is 0.999998958333873 or 1-0.00000104166613  @4096
//1s   is 0.999989583387583 or 1-0.00001041661242  @2048
//0.1s is 0.999895838758489 or 1-0.00010416124151  @1024
//10ms is 0.99895838758489  or 1-0.0010416124151  @512


#define FREQNUDGE_LPF	0.995
#define FREQCV_LPF	0.980

#define LAG_ATTACK_MIN_LPF 0.90
#define LAG_DECAY_MIN_LPF 0.95

#define ADC_POT_LPF		0.99
#define ADC_JACK_LPF	0.90

#define QPOT_MIN_CHANGE 100
#define QPOT_LPF 0.995

#define QLOCK_FLASH_SPEED 1500

#define LOCK_BUTTON_QUNLOCK_HOLD_TIME 50000

#define LOCK_BUTTON_LONG_HOLD_TIME 80000

#define SPREAD_ADC_HYSTERESIS 75


void param_read_freq_nudge(void);
void param_read_channel_level(void);
void param_poll_switches(void);
void param_read_q(void);
inline void update_lock_leds(void);
inline uint8_t num_locks_pressed(void);
void param_read_lock_buttons(void);

enum Env_Out_Modes{
ENV_SLOW,
ENV_FAST,
ENV_TRIG

};

#endif /* PARAMS_H_ */

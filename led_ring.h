/*
 * led_ring.h
 *
 *  Created on: Jun 29, 2015
 *      Author: design
 */

#ifndef LED_RING_H_
#define LED_RING_H_

void display_filter_rotation(void);
void display_scale(void);
void display_spectral_readout(void);
void do_assign_colors(void);
void update_LED_ring(int16_t change_scale_mode);

void exit_select_colors_mode(void);
void exit_assign_color_mode(void);
void enter_assign_color_mode(void);
void read_assigned_colors(void);
void write_assigned_colors(void);
void do_assign_colors(void);

#endif /* LED_RING_H_ */

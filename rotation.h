/*
 * rotation.h
 *
 *  Created on: Apr 1, 2015
 *      Author: design
 */

#ifndef ROTATION_H_
#define ROTATION_H_
void update_spread(uint8_t force_update);

inline void rotate_down(void);
inline void rotate_up(void);

uint32_t change_scale_up(void);
uint32_t change_scale_down(void);
void jump_scale_with_cv(int8_t shift_amt);

inline uint8_t is_morphing(void);
inline uint8_t is_spreading(void);
#endif /* ROTATION_H_ */

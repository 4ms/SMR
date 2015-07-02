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

uint32_t change_scale_up(uint32_t t_scalecv, uint32_t t_old_scalecv);
uint32_t change_scale_down(uint32_t t_scale_cv, uint32_t t_old_scalecv);


#endif /* ROTATION_H_ */

/*
 * rotation.h - Calculate position of each filter in the scale, based on rotation, spread, rotation CV, and scale CV
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




#ifndef ROTATION_H_
#define ROTATION_H_
void update_spread(uint8_t force_update);

void update_morph(void);
void update_motion(void);


void rotate_down(void);
void rotate_up(void);

void change_scale_up(void);
void change_scale_down(void);
void jump_scale_with_cv(int8_t shift_amt);

uint8_t is_morphing(void);
uint8_t is_spreading(void);

void jump_rotate_with_cv(int8_t shift_amt);

#endif /* ROTATION_H_ */

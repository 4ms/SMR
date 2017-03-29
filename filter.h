/*
 * filter.h - DSP bandpass resonant filter
 *
 * Author: Dan Green (danngreen1@gmail.com), Hugo Paris (hugoplho@gmail.com)
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


#ifndef FILTER_H_
#define FILTER_H_

#include "globals.h"
// Q LPF
#define QNUM 50					// number of q values in buffer of Q LPF
#define QLPF_UPDATEPERIOD 5 	// Low-passed qval updated every QLPF_UPDATEPERIOD

// CROSSFADE
#define CROSSFADE_POINT 4095 * 2 / 3	
#define CROSSFADE_WIDTH 1800				
#define CF_MIN (CROSSFADE_POINT - CROSSFADE_WIDTH/2)
#define CF_MAX (CROSSFADE_POINT + CROSSFADE_WIDTH/2)

void change_filter_type(enum Filter_Types newtype);
//void change_filter_mode(enum Filter_Modes newmode);

void process_audio_block(int16_t *src, int16_t *dst, uint16_t ht);

#endif /* FILTER_H_ */

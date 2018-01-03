/*
 * lpf.h - Low Pass Filter utilities
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


#pragma once

#include <stm32f4xx.h>

#define 	MAX_FIR_LPF_SIZE 40


enum AnalogPolarity{
	AP_UNIPOLAR,
	AP_BIPOLAR
};

typedef struct o_analog {
	//Value outputs:
	uint16_t			raw_val;
	float				lpf_val;
	int16_t				bracketed_val;

	//Settings (input)
	uint16_t			iir_lpf_size;		//size of iir average. 0 = disabled. if fir_lpf_size > 0, then iir average is disabled.
	uint16_t			fir_lpf_size; 		//size of moving average (number of samples to average). 0 = disabled.
	int16_t				bracket_size;		//size of bracket (ignore changes when old_val-bracket_size < new_val < old_val+bracket_size)
	enum AnalogPolarity	polarity;			//AP_UNIPOLAR or AP_BIPOLAR

	//Filter window buffer and index
	int32_t 			fir_lpf[ MAX_FIR_LPF_SIZE ];
	uint32_t 			fir_lpf_i;

} o_analog;



void setup_fir_filter(o_analog *a);
void apply_fir_lpf(o_analog *a);
void apply_bracket(o_analog *a);

/*
 * lpf.c - Low Pass Filter utilities
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

#include "lpf.h"

#define NUM_ANALOG_ELEMENTS 2

//
// FIR
//

void setup_fir_filter(o_analog *a)
{
	uint8_t i;

	//range check the lpf filter size
	if (a->fir_lpf_size > MAX_FIR_LPF_SIZE) a->fir_lpf_size = MAX_FIR_LPF_SIZE;

	//initialize the moving average buffer
	for (i=0; i<MAX_FIR_LPF_SIZE; i++)
	{
		//Bipolar CVs default to value of 2048
		//Other CVs default to 0
		if (a->polarity == AP_BIPOLAR)
		{
			a->fir_lpf[i] = 2048;
		} else {
			a->fir_lpf[i] = 0;
		}
	}

	//set inital values
	a->lpf_val = a->raw_val = a->bracketed_val = a->fir_lpf[0];
}

void apply_fir_lpf(o_analog *a)
{
	float old_value, new_value;

	old_value = a->fir_lpf[ a->fir_lpf_i ]; 	//oldest value: remove this from the array
	new_value = a->raw_val;										//new value: insert this into the array

	a->fir_lpf[ a->fir_lpf_i ] = new_value;

	//Increment the index, wrapping around the whole buffer
	if (++a->fir_lpf_i >= a->fir_lpf_size) a->fir_lpf_i = 0;

	//Calculate the arithmetic average (FIR LPF)
	a->lpf_val = (float)((a->lpf_val * a->fir_lpf_size) - old_value + new_value) / (float)(a->fir_lpf_size);

	//Range check 
	if (a->lpf_val < 0.0) a->lpf_val = 0.0;
	else if (a->lpf_val > 4095.0) a->lpf_val = 4095.0;
}


//
// Bracketing
//

void apply_bracket(o_analog *a)
{
	int16_t diff;

	diff = (int16_t)a->lpf_val - a->bracketed_val;

	if (diff > a->bracket_size)
	{
		a->bracketed_val = (uint16_t)a->lpf_val - a->bracket_size;
	}

	else if (diff < (-1*(a->bracket_size)))
	{
		a->bracketed_val = (uint16_t)a->lpf_val + a->bracket_size;
	}

	if (a->bracketed_val<0) 		a->bracketed_val=0;
	else if (a->bracketed_val>4095) a->bracketed_val=4095;

}


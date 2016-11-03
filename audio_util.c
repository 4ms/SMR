/*
 * audio_util.c - Audio processing routines
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

#include "audio_util.h"
#include "globals.h"
#include "filter.h"
#include "dig_inouts.h"

/* @brief	Splits a stereo 16-bit array into an array
 * 			The src array will have four times as many elements as the ldst or rdst
 *
 * @param	sz is the number of elements in the src array. Should be a multiple of 4
 * 			src is an array of 16-bit elements arranged Lmsb Llsb Rmsb Rlsb Lmsb Llsb Rmsb Rlsb
 * 			ldst and rdst are arrays of 32-bit elements that really just have 24 significant bits
 */
void audio_convert_2x16_to_stereo24(uint16_t sz, int16_t *src, int32_t *ldst, int32_t *rdst)
{

	while(sz)
	{



		*ldst = ((int32_t)(*src++))<<16;
		*ldst += *src++;

		if (*ldst>INPUT_LED_CLIP_LEVEL)
			LED_CLIPL_ON;
		else
			LED_CLIPL_OFF;
		*ldst++;

		*rdst = ((int32_t)(*src++))<<16;
		*rdst += *src++;
		if (*rdst>INPUT_LED_CLIP_LEVEL)
			LED_CLIPR_ON;
		else
			LED_CLIPR_OFF;
		*rdst++;
		
		sz-=4;
	}

}

void audio_convert_stereo24_to_2x16(uint16_t sz, int32_t *lsrc, int32_t *rsrc, int16_t *dst)
{
	while(sz)
	{
		*dst++=(int16_t)(*lsrc >> 16);
		*dst++=(int16_t)(*lsrc++ & 0x0000FFFF);
		//*lsrc++;
		*dst++=(int16_t)(*rsrc >> 16);
		*dst++=(int16_t)(*rsrc++ & 0x0000FFFF);
		//*rsrc++;

		sz-=4;
	}

}




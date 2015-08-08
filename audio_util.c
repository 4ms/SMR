/*
 * audio_delay.c - Audio processing routines
 */

#include "audio_util.h"
#include "globals.h"
#include "filter.h"
#include "inouts.h"

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
		*ldst = ((int32_t)(*src))<<16;
		*src++;
		*ldst += *src;
		*src++;
		*ldst++;

		*rdst = ((int32_t)(*src++))<<16;
		*rdst += *src++;
		*rdst++;

		sz-=4;
	}
}

void audio_convert_stereo24_to_2x16(uint16_t sz, int32_t *lsrc, int32_t *rsrc, int16_t *dst)
{
	while(sz)
	{
		*dst++=(int16_t)(*lsrc >> 16);
		*dst++=(int16_t)(*lsrc & 0x0000FFFF);
		*lsrc++;
		*dst++=(int16_t)(*rsrc >> 16);
		*dst++=(int16_t)(*rsrc & 0x0000FFFF);
		*rsrc++;

		sz-=4;
	}

}




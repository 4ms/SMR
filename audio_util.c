/*
 * audio_delay.c - Audio processing routines
 */

#include "audio_util.h"
//#include "memory.h"
#include "globals.h"
#include "i2s.h" //we just need to know the codec_BUFF_LEN from here
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

/* @brief	Splits a stereo 16-bit array into an array containing the left channel arranged in two 8-bit elements
 * 			The src and dst arrays will have the same number of elements
 * 			but dst's elements are 8-bits and src's elements are 16-bits
 * @param	sz is the number of elements in the src array. Should be an even number
 * 			src is an array of 16-bit elements arranged L R L R L R
 * 			dst is an array of 8-bit elements arranged LlLlLl where "L" is the MSByte and "l" is the LSByte
 *
 */
void audio_split_justleft_16to8(uint16_t sz, int16_t *src, int8_t *dst)
{
	while(sz!=0 && sz!=1){
		*dst++ = (*src >> 8); //copy the left channel MSByte
		*dst++ = (*src++ & 0x00FF); //copy the left channel LSByte
		*src++; //skip the right channel word
		sz-=2;
	}
}

/* @brief	Splits a stereo signed 16-bit array (0 = Center, 0x7FFF = Max) into unsigned array containing just the left channel (0x8000 = Center, 0x0000 = Min, 0xFFFF = Max).
 * 			The dst array will have sz/2 elements, and the src array should have sz elements.
 * @param	sz is the number of elements in the src array.
 * 			src is an array of 16-bit elements arranged L R L R L R...
 * 			dst is an array of 16-bit elements that's just L L L...
 *
 */

void audio_split_justleft(uint16_t sz, int16_t *src, uint16_t *dst)
{
	while(sz){
		*dst++ = (*src++) + 0x8000; //copy the left channel word
		*src++; //skip the right channel word
		sz-=2;
	}
}

void audio_stereoize_justleft(int16_t sz, int16_t *dst, int16_t *lsrc)
{
	while(sz)
	{
		*dst++ = *lsrc++;
		sz--;
		*dst++ = 0;
		sz--;
	}
}





/** Audio_Init()
  * @brief  This function sets up audio processing.
  * @param  none
  * @retval none
  */
void Audio_Init(void)
{
}


/**
  * @brief  Split interleaved stereo into two separate buffers
  * @param  sz -  samples per input buffer (divisible by 2)
  * @param  src - pointer to source buffer
  * @param  ldst - pointer to left dest buffer (even samples)
  * @param  rdst - pointer to right dest buffer (odd samples)
  * @retval none
  */
void audio_split_stereo(int16_t sz, int16_t *src, int16_t *ldst, int16_t *rdst)
{
	while(sz)
	{
		*ldst++ = *src++;
		sz--;
		*rdst++ = *src++;
		sz--;
	}
}

/**
  * @brief  combine two separate buffers into interleaved stereo
  * @param  sz -  samples per output buffer (divisible by 2)
  * @param  dst - pointer to source buffer
  * @param  lsrc - pointer to left dest buffer (even samples)
  * @param  rsrc - pointer to right dest buffer (odd samples)
  * @retval none
  */
void audio_comb_stereo(int16_t sz, int16_t *dst, int16_t *lsrc, int16_t *rsrc)
{
	while(sz)
	{
		*dst++ = *lsrc++;
		sz--;
		*dst++ = *rsrc++;
		sz--;
	}
}



/**
  * @brief  morph a to b into destination
  * @param  sz -  samples per buffer
  * @param  dst - pointer to source buffer
  * @param  asrc - pointer to dest buffer
  * @param  bsrc - pointer to dest buffer
  * @param  morph - float morph coeff. 0 = a, 1 = b
  * @retval none
  */
void audio_morph(int16_t sz, int16_t *dst, int16_t *asrc, int16_t *bsrc,
				float32_t morph)
{
	float32_t morph_inv = 1.0 - morph, f_sum;
	int32_t sum;
	
	while(sz--)
	{
		f_sum = (float32_t)*asrc++ * morph_inv + (float32_t)*bsrc++ * morph;
		sum = f_sum;
#if 0
		sum = sum > 32767 ? 32767 : sum;
		sum = sum < -32768 ? -32768 : sum;
#else
		asm("ssat %[dst], #16, %[src]" : [dst] "=r" (sum) : [src] "r" (sum));
#endif
		
		/* save to destination */
		*dst++ = sum;
	}
}





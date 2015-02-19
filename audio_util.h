/*
 * audio.h - audio processing routines
 */

#ifndef __audio__
#define __audio__

//#define ARM_MATH_CM4

#include "stm32f4xx.h"
#include "arm_math.h"


void Audio_Init(void);
void audio_stereoize_justleft(int16_t sz, int16_t *dst, int16_t *lsrc);

void audio_split_justleft(uint16_t sz, int16_t *src, uint16_t *dst);
void audio_convert_2x16_to_stereo24(uint16_t sz, int16_t *src, int32_t *ldst, int32_t *rdst);


#endif


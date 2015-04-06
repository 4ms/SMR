/*
 * globals.h
 *
 *  Created on: Jun 8, 2014
 *      Author: design
 */

#ifndef GLOBALS_H_
#define GLOBALS_H_


#define NUM_FILTS 20
#define NUM_CHANNELS 6

#define POST 1
#define PRE 0

#define SHOW_CLIPPING 0
#define SHOW_LEVEL 1


#define codec_BUFF_LEN 128


#define OUT_OF_MEM (1<<0)
#define OUT_OF_SAMPLES (1<<1)
#define SPIERROR_1 (1<<2)
#define SPIERROR_2 (1<<3)
#define DMA_OVR_ERROR (1<<4)
#define sFLASH_BAD_ID (1<<5)
#define WRITE_BUFF_OVERRUN (1<<6)
#define READ_BUFF_OVERRUN (1<<7)

#define NUMSCALES 11
#define NUMSCALEBANKS 6

//#define SAMPLERATE 24000
//#define SAMPLERATE 48000
#define SAMPLERATE 96000


#endif /* GLOBALS_H_ */

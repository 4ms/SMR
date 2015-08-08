/*
 * globals.h
 *
 *  Created on: Jun 8, 2014
 *      Author: design
 */

#ifndef GLOBALS_H_
#define GLOBALS_H_

#define FW_VERSION 0x01

//Number of components
#define NUM_FILTS 20
#define NUM_CHANNELS 6
#define NUMSCALES 11
#define NUMSCALEBANKS 16
#define NUM_COLORSCHEMES 16

//Parameter values
#define SHOW_CLIPPING 0
#define DONT_SHOW_CLIPPING 1
#define SLIDER_CLIP_LEVEL 0x50000000
#define INPUT_LED_CLIP_LEVEL 0x58000000


enum Filter_Types {
	BPRE,
	MAXQ
};

#define PRE 0
#define POST 1

//Backend
#define codec_BUFF_LEN 128

//#define SAMPLERATE 24000 /*Use only for debugging*/
#define SAMPLERATE 96000


#endif /* GLOBALS_H_ */

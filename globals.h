/*
 * globals.h
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



#ifndef GLOBALS_H_
#define GLOBALS_H_

#define FW_VERSION 0x04

//Number of components
#define NUM_FILTS 20
#define NUM_CHANNELS 6
#define NUMSCALES 11
#define NUMSCALEBANKS 20
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

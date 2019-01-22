/*
 * contemplation.c - implementation of the contemplation mode
 *
 * Author: Jon Butler
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

#include "stm32f4xx.h"
#include "contemplation.h"
#include "globals.h"
#include "dig_inouts.h"

const float max_slider_val = 4096.0f;
float contemplation_level[NUM_CHANNELS] = {0,0,0,0,0,0};
uint8_t exp_curve_index[NUM_CHANNELS] = {0,0,0,0,0,0};

extern uint16_t potadc_buffer[NUM_ADC3S];
extern uint32_t env_prepost_mode;
extern uint32_t ENVOUT_PWM[NUM_CHANNELS];
extern float exp_4096[4096];

void init_contemplation() {
    RCC_AHB2PeriphClockCmd(RCC_AHB2Periph_RNG, ENABLE);
    RNG_Cmd(ENABLE);
}

// Utility function to get a random number from the hardware RNG. Blocks, but we should do
// this infrequently enough not to be a problem
uint32_t getRand() {
    while(1) {
        if(RNG_GetFlagStatus(RNG_FLAG_DRDY) == SET) {
            break;
        }

        if(RNG_GetFlagStatus(RNG_FLAG_CECS) == SET || RNG_GetFlagStatus(RNG_FLAG_SECS) == SET) {
            // Reset on error - this is untested
            RNG_DeInit();
            init_contemplation();
        }
    }

    return RNG_GetRandomNumber();
}

void contemplate() {
    // Time to contemplate! For each channel, calculate the target level as a float,
    // then assign the normalised uint32_t value to the pwm output.
    for (uint8_t j = 0; j < NUM_CHANNELS; j++) {
        uint32_t slider_val = potadc_buffer[j+SLIDER_ADC_BASE];
        float scaled_slider_val = (float)slider_val / max_slider_val;

        uint32_t r = getRand();
        // Optionally re-trigger if a few conditions are met:
        //	We're in pre-contemplation mode
        //	The slider is at the top 1/4 of it's range
        //	We're at least half decayed from the existing note
        uint8_t should_retrig = (env_prepost_mode==PRE && 
                                scaled_slider_val > 0.75 &&
                                (r % 200) == 0 &&
                                contemplation_level[j] < (max_slider_val * 0.5));

        if (contemplation_level[j] == 0 || should_retrig) {
            // Don't trigger if the level slider is down
            if(scaled_slider_val < 0.01) {
                // Make sure we're actually at zero (sometimes we're not, e.g.
                // when we've just changed modes)
                contemplation_level[j] = 0;
                ENVOUT_PWM[j] = 0;
                continue;
            }

            // There are different trigger modes based on pre/post:
            if (env_prepost_mode==PRE) {
                //	1 - Fixed volume, sliders control likelihood
                if(should_retrig || ((r % 4096) < (slider_val / 32))) {
                    contemplation_level[j] = max_slider_val;
                    exp_curve_index[j] = 0;
                }
            } else {
                //	2 - Fixed likelihood, sliders control volume.
                contemplation_level[j] = (float)slider_val;
                exp_curve_index[j] = 0;
            }	
        } else {
            // We're currently triggered, tail off exponentially
            contemplation_level[j] *= exp_4096[exp_curve_index[j]];

            // Only advance the tail off amount roughly 33% of the time,
            // to make the notes ring out longer
            if((r % 2) == 0) {
                exp_curve_index[j]++;
            }
            
            
            // Make sure we've not gone off the end of the curve, or neared the end
            // of the float range
            if (exp_curve_index[j] >= 4096 || contemplation_level[j] <= 0.0001) {
                contemplation_level[j] = 0;
                exp_curve_index[j] = 0;
            }
        }

        // Finally, set the PWM output to the rounded version of the current contemplation level
        ENVOUT_PWM[j] = (uint32_t)contemplation_level[j];
    }
}

/*
 * rotation.c - Calculate position of each filter in the scale, based on rotation, spread, rotation CV, and scale CV
 *
 * Author: Dan Green (danngreen1@gmail.com)
 * 2015
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


#include <stm32f4xx.h>
#include "globals.h"
#include "dig_inouts.h"
#include "exp_4096.h"
#include "system_mode.h"


extern __IO uint16_t adc_buffer[NUM_ADCS+1];
extern uint8_t lock[NUM_CHANNELS];

int8_t motion_fadeto_note[NUM_CHANNELS];
int8_t motion_fadeto_scale[NUM_CHANNELS];
//int8_t motion_fadeto_bank[NUM_CHANNELS];

int8_t motion_rotate;
int8_t motion_spread_dest[NUM_CHANNELS];
int8_t motion_spread_dir[NUM_CHANNELS];

int8_t motion_notejump;
int8_t motion_scale_dest[NUM_CHANNELS];
int8_t motion_scalecv_overage[NUM_CHANNELS]={0,0,0,0,0,0};

float motion_morphpos[NUM_CHANNELS]={0.0,0.0,0.0,0.0,0.0,0.0};


extern uint8_t note[NUM_CHANNELS];
extern uint8_t scale[NUM_CHANNELS];

extern uint16_t rotate_to_next_scale;
extern uint8_t flag_update_LED_ring;



int8_t spread=0;
float rot_dir[6];
uint8_t scale_bank_defaultscale[NUMSCALEBANKS]={4,4,6,5,9,5,5};


void change_scale_up(void){
	uint8_t i;

	for (i=0;i<NUM_CHANNELS;i++) {
		if (lock[i]!=1) {
			if (motion_scale_dest[i]<(NUMSCALES-1))
				motion_scale_dest[i]++;

		}
	}

}

void change_scale_down(void){
	uint8_t i;

	for (i=0;i<NUM_CHANNELS;i++) {
		if (lock[i]!=1) {
			if (motion_scale_dest[i]>0) {
				motion_scale_dest[i]--;
			}
		}
	}
}

void jump_rotate_with_cv(int8_t shift_amt){

	motion_notejump+=shift_amt;

}


void jump_scale_with_cv(int8_t shift_amt){
	uint8_t i;
	int8_t this_shift_amt;

	for (i=0;i<NUM_CHANNELS;i++) {

		if (lock[i]!=1) {
			this_shift_amt=shift_amt;

			if (this_shift_amt<0 && motion_scalecv_overage[i]>0){
				if ((-1*this_shift_amt) > motion_scalecv_overage[i]){
					this_shift_amt += motion_scalecv_overage[i];
					motion_scalecv_overage[i] = 0;
				} else {
					motion_scalecv_overage[i] += this_shift_amt;
					this_shift_amt = 0;
				}
			}

			motion_scale_dest[i] += this_shift_amt;

			if (motion_scale_dest[i]<0) {
				motion_scale_dest[i]=0;
			}
			if (motion_scale_dest[i]>(NUMSCALES-1)){
				motion_scalecv_overage[i]=motion_scale_dest[i]-(NUMSCALES-1);
				motion_scale_dest[i]=NUMSCALES-1;
			}
		}
	}
}

inline void rotate_down(void){

	//if we were rotating down, reverse direction. Otherwise add to current direction
	if (motion_rotate>=0) motion_rotate=-1;
	else motion_rotate--;

}

inline void rotate_up(void){

	if (motion_rotate<=0) motion_rotate=1;
	else motion_rotate++;


}

inline uint8_t is_spreading(void){
	if (motion_spread_dir[0]==0
			&& motion_spread_dir[1]==0
			&& motion_spread_dir[2]==0
			&& motion_spread_dir[3]==0
			&& motion_spread_dir[4]==0
			&& motion_spread_dir[5]==0)
		return(0);
	else return(1);
}

inline uint8_t is_morphing(void){
	if (motion_morphpos[0]==0.0
			&& motion_morphpos[1]==0.0
			&& motion_morphpos[2]==0.0
			&& motion_morphpos[3]==0.0
			&& motion_morphpos[4]==0.0
			&& motion_morphpos[5]==0.0)
		return(0);
	else return(1);

}


void update_spread(int8_t t_spread){
	int8_t spread_change;
	static int8_t old_spread=1;

	float spread_out;
	int32_t i, test_spot, offset, test_scale, base_note;
	uint8_t j;
	uint8_t is_distinct;

	int32_t test_motion[6]={0,0,0,0,0,0};

	base_note=motion_fadeto_note[2];

	spread=t_spread;

	if (spread>old_spread) spread_out=1;
	else spread_out=-1;

	old_spread=spread;

	test_motion[0]=99;
	test_motion[1]=99;
	test_motion[3]=99;
	test_motion[4]=99;
	test_motion[5]=99;

	base_note=motion_fadeto_note[2];

	for (i=0;i<NUM_CHANNELS;i++){

		if (lock[i]==1 || i==2){
			test_motion[i]=motion_fadeto_note[i];

		} else {
			//Set spread direction. Note spread_dir[2] remains 0 (stationary)
			if (i<2) motion_spread_dir[i]=-1*spread_out;
			if (i>2) motion_spread_dir[i]=spread_out;

			//Find an open filter channel:
			//Our starting point is based on the location of channel 2, and spread outward from there
			offset = (((int32_t)i) - 2) * spread;
			test_spot = base_note + offset;

			//Set up test_spot, since the first thing we do is in the loop is test_spot += spread_dir[i]
			test_spot -= motion_spread_dir[i];

			//Now check to see if test_spot is open
			is_distinct=0;
			while (!is_distinct){

				test_spot += motion_spread_dir[i];

				while (test_spot >= NUM_FILTS){
					test_spot -= NUM_FILTS;
				}
				while (test_spot < 0){
					test_spot += NUM_FILTS;
				}

				// Check to see if test_spot is a distinct value:

				for (is_distinct=1,j=0;j<NUM_CHANNELS;j++){
					//...if the test spot is already assigned to a locked or stationary channel (2), or a channel we already spread'ed, then try again
					if (i!=j && ((test_spot==test_motion[j] && j<i) || (test_spot==motion_fadeto_note[j] && (lock[j]==1 || j==2))) )
						is_distinct=0;
				}
			}

			test_motion[i] = test_spot;

		}
	}

	motion_spread_dest[0]=test_motion[0];
	motion_spread_dest[1]=test_motion[1];
	motion_spread_dest[2]=test_motion[2];
	motion_spread_dest[3]=test_motion[3];
	motion_spread_dest[4]=test_motion[4];
	motion_spread_dest[5]=test_motion[5];

}




inline void update_motion(void){
	float f_morph;
	uint16_t chan;
	uint16_t test_chan;
	int16_t i;
	uint8_t is_distinct;


	//f_morph is how fast we morph (cross-fade increment)
	f_morph=exp_4096[(adc_buffer[MORPH_ADC])];

	for (chan=0;chan<NUM_CHANNELS;chan++){

		//if morph is happening, continue it
		if (motion_morphpos[chan]>0)
			motion_morphpos[chan] += f_morph;

		//if morph has reached the end, shift our present position to the (former) fadeto destination
		if (motion_morphpos[chan]>=1.0){
			note[chan] = motion_fadeto_note[chan];
			scale[chan] = motion_fadeto_scale[chan];

			//scale_bank[chan] = motion_fadeto_bank[chan];

			if (motion_spread_dest[chan] == note[chan]) motion_spread_dir[chan] = 0;

			motion_morphpos[chan]=0.0;

			flag_update_LED_ring=1;
		}
	}



	//Initiate a motion if one is not happening

	if (!is_morphing()){

	//Rotation CW
		if (motion_rotate>0){
			motion_rotate--;
			for (chan=0;chan<6;chan++){
				if (!lock[chan]){
					motion_spread_dir[chan]=1;
					//Increment circularly
					if (motion_spread_dest[chan]>=(NUM_FILTS-1))
						motion_spread_dest[chan]=0;
					else
						motion_spread_dest[chan]++;
				}
			}
		} else

	//Rotation CCW
		if (motion_rotate<0){
			motion_rotate++;
			for (chan=0;chan<6;chan++){
				if (!lock[chan]){
					motion_spread_dir[chan]=-1;
					//Dencrement circularly
					if (motion_spread_dest[chan]==0)
						motion_spread_dest[chan]=NUM_FILTS-1;
					else
						motion_spread_dest[chan]--;
				}
			}
		}else

	//Rotate CV (jump)
		if (motion_notejump!=0){

			for (chan=0;chan<6;chan++){
				if (!lock[chan]){

					//Dec/Increment circularly
					motion_fadeto_note[chan]+=motion_notejump;
					while (motion_fadeto_note[chan]<0) motion_fadeto_note[chan]+=NUM_FILTS;
					while (motion_fadeto_note[chan]>=NUM_FILTS) motion_fadeto_note[chan] -= NUM_FILTS;

					//Dec/Increment circularly
					motion_spread_dest[chan]+=motion_notejump;
					while (motion_spread_dest[chan]<0) motion_spread_dest[chan]+=NUM_FILTS;
					while (motion_spread_dest[chan]>=NUM_FILTS) motion_spread_dest[chan] -= NUM_FILTS;


					//Check if the new destination is occupied by a locked channel or a channel we already notejump'ed
					for (is_distinct=1,test_chan=0;test_chan<NUM_CHANNELS;test_chan++){
						if (chan!=test_chan && motion_fadeto_note[chan]==motion_fadeto_note[test_chan] && (lock[test_chan] || test_chan<chan))
							is_distinct=0;
					}
					while (!is_distinct){

						//Inc/decrement circularly

						if (motion_notejump>0){
							motion_fadeto_note[chan]=(motion_fadeto_note[chan] + 1) % NUM_FILTS;
						}else {
							if (motion_fadeto_note[chan]==0) motion_fadeto_note[chan] = NUM_FILTS-1;
							else motion_fadeto_note[chan] = motion_fadeto_note[chan] - 1;
						}

						//Check if the new destination is occupied by a locked channel or a channel we already notejump'ed
						for (is_distinct=1,test_chan=0;test_chan<NUM_CHANNELS;test_chan++){
							if (chan!=test_chan && motion_fadeto_note[chan]==motion_fadeto_note[test_chan] && (lock[test_chan] || test_chan<chan))
								is_distinct=0;
						}
					}
					//Start the motion morph
					motion_morphpos[chan] = f_morph;
					motion_fadeto_scale[chan]=motion_scale_dest[chan];

				}
			}
			motion_notejump=0;
		}
	}


	for (chan=0;chan<NUM_CHANNELS;chan++){

		if (motion_morphpos[chan]==0.0){

	//Spread
			//Problem: try spreading from 5 to 6, then 6 to 5.
			//channel 0 and 3 collide like this:
			//Start 18 x x 16 x x
			//End   16 x x 17 x x
			//So channel 0 starts to fade from 18 to 17
			//Channel 3 wants to fade to 17, but 17 is taken as a fadeto, so it fades to 18.
			//Next step:
			//Channel 0 fades to 16 and is done
			//Channel 3 fades from 18 to 19, 0, 1...17. Since its dir is set to +1, it has to go all the way around the long way from 18 to 17
			//Not big problem except that the scale is incremented on channel 3, and when spread is returned to 5, the scale does not decrement
			//Thus channel 3 will be in a different scale than when it started.
			//Solution ideas:
			//-ignore the issue, but force spread=1 to make all notes in the same scale (unless spanning 19/0)
			//-allow channels to fadeto the same spot, but not have the same motion_spread_dest
			// --ch0: 18->17 ch3: 16->17; ch0: 17->16 ch3: stay
			//-change the is_distinct code in fadeto to give priority to channels that are hitting their motion_spread_dest
			// --or somehow change it so ch0: 18->17, ch3:16->17 bumps ch0 18->16

			if (motion_spread_dest[chan] != note[chan]){

	//Check if the spread destination is still available

				for (is_distinct=1,test_chan=0;test_chan<NUM_CHANNELS;test_chan++){
					if (chan!=test_chan && (motion_spread_dest[chan]==motion_spread_dest[test_chan]) && (lock[test_chan]==1 || test_chan<chan || motion_spread_dir[test_chan]==0))
						is_distinct=0;
				}
				while (!is_distinct){

					//Inc/decrement circularly
					if (motion_spread_dir[chan]==0)
						motion_spread_dir[chan]=1;

					if (motion_spread_dir[chan]>0){
						motion_spread_dest[chan]=(motion_spread_dest[chan] + 1) % NUM_FILTS;
					}else {
						if (motion_spread_dest[chan]==0) motion_spread_dest[chan] = NUM_FILTS-1;
						else motion_spread_dest[chan] = motion_spread_dest[chan] - 1;
					}

					//Check that it's not already taken by a locked channel, channel with a higher priority, or a non-moving channel
					for (is_distinct=1,test_chan=0;test_chan<NUM_CHANNELS;test_chan++){
						if (chan!=test_chan && (motion_spread_dest[chan]==motion_spread_dest[test_chan]) && (lock[test_chan]==1 || test_chan<chan || motion_spread_dir[test_chan]==0))
							is_distinct=0;
					}
				}


	//Clockwise Spread
				if (motion_spread_dir[chan] > 0) {

					//Start the motion morph
					motion_morphpos[chan] = f_morph;

					// Shift the destination CW, wrapping it around
					is_distinct=0;
					while (!is_distinct){

						//Increment circularly
						if (motion_fadeto_note[chan]>=(NUM_FILTS-1)){
							motion_fadeto_note[chan]=0;

							// If scale rotation is on, increment the scale, wrapping it around
							if (rotate_to_next_scale){
								motion_fadeto_scale[chan]=(motion_fadeto_scale[chan]+1) % NUMSCALES;
								motion_scale_dest[chan]=(motion_scale_dest[chan]+1) % NUMSCALES;
							}
						}
						else
							motion_fadeto_note[chan]++;

						//Check that it's not already taken by a locked channel, channel with a higher priority, or a non-moving channel
						for (is_distinct=1,test_chan=0;test_chan<NUM_CHANNELS;test_chan++){
							if (chan!=test_chan && (motion_fadeto_note[chan]==motion_fadeto_note[test_chan]) && (lock[test_chan]==1 || test_chan<chan))
								is_distinct=0;
						}
					}
				} else

	//Counter-clockwise Spread
				if (motion_spread_dir[chan]<0){

					//Start the motion morph
					motion_morphpos[chan] = f_morph;

					// Shift the destination CCW, wrapping it around and repeating until we find a distinct value
					is_distinct=0;
					while (!is_distinct){

						//Decrement circularly
						if (motion_fadeto_note[chan]==0) {
							motion_fadeto_note[chan] = NUM_FILTS-1;

							// If scale rotation is on, decrement the scale, wrapping it around
							if (rotate_to_next_scale){
								if (motion_fadeto_scale[chan]==0) motion_fadeto_scale[chan]=NUMSCALES-1;
								else motion_fadeto_scale[chan]--;
								if (motion_scale_dest[chan]==0) motion_scale_dest[chan]=NUMSCALES-1;
								else motion_scale_dest[chan]--;

							}
						}
						else
							motion_fadeto_note[chan]--;

						//Check that it's not already taken by a locked channel, channel with a higher priority, or a non-moving channel
						for (is_distinct=1,test_chan=0;test_chan<NUM_CHANNELS;test_chan++){
							if (chan!=test_chan && (motion_fadeto_note[chan]==motion_fadeto_note[test_chan]) && (lock[test_chan]==1 || test_chan<chan))
								is_distinct=0;
						}
					}
				}
			}
			else

	//Scale
			if (motion_scale_dest[chan] != motion_fadeto_scale[chan]){
				//Start the motion morph
				motion_morphpos[chan] = f_morph;

				motion_fadeto_scale[chan]=motion_scale_dest[chan];
			}
		}
	}
}

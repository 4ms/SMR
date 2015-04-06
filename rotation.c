/*
 * rotation.c
 *
 *  Created on: Apr 1, 2015
 *      Author: design
 */

#include "stm32f4xx.h"
#include "globals.h"
#include "inouts.h"

extern __IO uint16_t adc_buffer[NUM_ADCS+1];
extern uint8_t lock[NUM_CHANNELS];
extern uint8_t dest_filter_assign_table[NUM_CHANNELS];

extern uint8_t scale[NUM_CHANNELS];
extern uint8_t dest_scale[NUM_CHANNELS];
extern uint8_t scale_bank[NUM_CHANNELS];

extern uint16_t rotate_to_next_scale;


#define SPREAD_ADC_HYSTERESIS 75

int8_t spread=99;
float rot_dir[6];
float spread_dir[6];
uint8_t scale_bank_defaultscale[NUMSCALEBANKS]={4,4,6,5,9,5};


uint32_t change_scale_up(uint32_t t_scalecv, uint32_t t_old_scalecv){
	uint8_t i;

	for (i=0;i<NUM_CHANNELS;i++) {
		if (lock[i]!=1) {
			if (scale[i]<(NUMSCALES-1)) {
				scale[i]++; //no wrap-around
				if (dest_scale[i]<(NUMSCALES-1))
					dest_scale[i]++;//dest_scale[i]=scale[i];?
			} else {
				t_old_scalecv=t_scalecv;
			}
			scale_bank_defaultscale[scale_bank[i]]=scale[i];
		}
	}

	return(t_old_scalecv);
}

uint32_t change_scale_down(uint32_t t_scale_cv, uint32_t t_old_scalecv){
	uint8_t i;


		for (i=0;i<NUM_CHANNELS;i++) {
			if (lock[i]!=1) {
				if (scale[i]>0)	{
					scale[i]--; //no wrap-around
					if (dest_scale[i]>0) dest_scale[i]--;//=scale[i];?
				}
				scale_bank_defaultscale[scale_bank[i]]=scale[i];
			}
		}


}

inline void rotate_down(uint8_t do_rotate_down){
	uint8_t i, j, is_distinct;

	//if (do_rotate_down){
	//	do_rotate_down--;

		for (i=0;i<NUM_CHANNELS;i++){
			//Don't change locked filters
			if (lock[i]!=1){

				rot_dir[i]=-1;

				//Find a distinct value, shifting downward if necessary
				is_distinct=0;
				while (!is_distinct){

					if (dest_filter_assign_table[i]==0) {
						dest_filter_assign_table[i] = NUM_FILTS-1;
						if (rotate_to_next_scale){
							if (scale[i]==0) dest_scale[i]=NUMSCALES-1;
							else dest_scale[i]=scale[i]-1;
						}
					}
					else
						dest_filter_assign_table[i]--;

					//Check if dest_filter_assign_table[i] is already taken by a locked channel
					for (is_distinct=1,j=0;j<NUM_CHANNELS;j++){
						if (i!=j && dest_filter_assign_table[i]==dest_filter_assign_table[j] && lock[j]==1)
							is_distinct=0;
					}
				}
			}
		}
	//}
}

inline void rotate_up(uint8_t do_rotate_up){
	uint8_t i, j, is_distinct;

	//if (do_rotate_up--){

		for (i=0;i<NUM_CHANNELS;i++){ //takes 4us

			//Don't change locked filters
			if (lock[i]!=1){

				rot_dir[i]=1;

				//Find a distinct value, shifting upward if necessary
				is_distinct=0;
				while (!is_distinct){
					//dest_filter_assign_table[i]=(dest_filter_assign_table[i]+1) % NUM_FILTS;
					//Loop it around to the bottom of the scale, or to bottom of the next scale if we're in RANGE BANK mode
					if (dest_filter_assign_table[i]==(NUM_FILTS-1)){
						dest_filter_assign_table[i]=0;
						if (rotate_to_next_scale) {
							dest_scale[i]=(scale[i]+1) % NUMSCALES;
						}
					} else
						dest_filter_assign_table[i]++;

					for (is_distinct=1,j=0;j<NUM_CHANNELS;j++){
						if (i!=j && dest_filter_assign_table[i]==dest_filter_assign_table[j] && lock[j]==1)
							is_distinct=0;
					}
				}
			}
		}
	//}
}


void update_spread(uint8_t force_update){
	static old_spread=1;
	float spread_out;
	uint8_t test_spread=0, hys_spread=0;
	uint16_t temp_u16;
	int32_t i, test_spot, offset, test_scale;
	uint8_t j;
	uint8_t is_distinct;


	// Check if spread ADC has changed values enough to warrant a change in parameter value (hystersis)

	test_spread=(adc_buffer[SPREAD_ADC] >> 8) + 1; //0-4095 to 1-16

	if (test_spread < spread){
		if (adc_buffer[SPREAD_ADC] <= (4095-SPREAD_ADC_HYSTERESIS))
			temp_u16 = adc_buffer[SPREAD_ADC] + SPREAD_ADC_HYSTERESIS;
		else
			temp_u16 = 4095;

		hys_spread = (temp_u16 >> 8) + 1;
		old_spread=spread;

	} else if (test_spread > spread){
		if (adc_buffer[SPREAD_ADC] > SPREAD_ADC_HYSTERESIS)
			temp_u16 = adc_buffer[SPREAD_ADC] - SPREAD_ADC_HYSTERESIS;
		else
			temp_u16 = 0;

		hys_spread = (temp_u16 >> 8) + 1;
		old_spread=spread;

	} else {
		hys_spread=0xFF; //adc has not changed, do nothing
	}


	// If it changed far enough "into" a new value of spread, then re-calculate the postion of all unlocked filters

	if (hys_spread == test_spread || force_update){

		spread=test_spread;

		if (spread>old_spread) spread_out=1;
		else spread_out=-1;

		for (i=0;i<NUM_CHANNELS;i++){

			if (lock[i]!=1){

				if (i<3) spread_dir[i]=-1.0*spread_out;
				else spread_dir[i]=spread_out;

				//Find an open filter channel:
				//Our starting point is based on the location of channel 2, and spread outward from there
				test_spot = (int32_t)dest_filter_assign_table[2];
				offset = (((int32_t)i) - 2) * spread;
				test_spot = test_spot + offset;

				if (rotate_to_next_scale){
					test_scale = dest_scale[2];
				}

				//Set up test_spot, since the first thing we do is in the loop is test_spot += spread_dir[i]
				test_spot -= spread_dir[i];

				//Now check to see if test_spot is open
				is_distinct=0;
				while (!is_distinct){

					test_spot += spread_dir[i];

					while (test_spot >= NUM_FILTS){
						test_spot -= NUM_FILTS;
						if (rotate_to_next_scale){
							test_scale=(test_scale+1) % NUMSCALES;
						}
					}
					while (test_spot < 0){
						test_spot += NUM_FILTS;
						if (rotate_to_next_scale){
							if (test_scale==0) test_scale=NUMSCALES-1;
							else test_scale=test_scale-1;
						}
					}

					// Check to see if test_spot is a distinct value:

					for (is_distinct=1,j=0;j<NUM_CHANNELS;j++){
						//...if the test spot is already assigned to a locked channel, or a channel we already spread'ed, then try again
						if (i!=j && test_spot==dest_filter_assign_table[j] && (lock[j]==1 || (j<i)))
							is_distinct=0;
					}
				}
				dest_filter_assign_table[i] = test_spot;
				if (rotate_to_next_scale) dest_scale[i] = test_scale;

			}
		}

	}



}


/*
		if (lock[5]==1) lock_test5=dest_filter_assign_table[5];
		else lock_test5=99;

		//Assign [0] and check for duplicate placement over 2 & lock5
		if (lock[0]!=1){
			if ((NUM_FILTS+dest_filter_assign_table[2])<(spread*2)) dest_filter_assign_table[0] = (NUM_FILTS*2)+dest_filter_assign_table[2]-(spread*2);
			else if (dest_filter_assign_table[2]<(spread*2)) dest_filter_assign_table[0] = NUM_FILTS+dest_filter_assign_table[2]-(spread*2);
			else dest_filter_assign_table[0]=dest_filter_assign_table[2]-(spread*2);
			rot_dir[0]=-1.0*spread_dir;

			while (dest_filter_assign_table[0]==lock_test5
					|| dest_filter_assign_table[0]==dest_filter_assign_table[2])
				dest_filter_assign_table[0]=(dest_filter_assign_table[0]+1) % NUM_FILTS;

		}

		//Assign [1] and check for duplicate placement over 0, 2 & lock5
		if (lock[1]!=1){
			if (dest_filter_assign_table[2]<spread) dest_filter_assign_table[1] = NUM_FILTS+dest_filter_assign_table[2]-spread;
			else dest_filter_assign_table[1]=dest_filter_assign_table[2]-spread;
			rot_dir[1]=-1.0*spread_dir;
			while (				dest_filter_assign_table[1]==dest_filter_assign_table[0]
								|| dest_filter_assign_table[1]==dest_filter_assign_table[2]
								|| dest_filter_assign_table[1]==lock_test5)
				dest_filter_assign_table[1]=(dest_filter_assign_table[1]+1) % NUM_FILTS;
		}


		//Keep [2] stationary
		rot_dir[2]=-1.0*spread_dir;


		//Assign [3] and check for duplicate placement over 0,1,2 & lock5
		if (lock[3]!=1){
			dest_filter_assign_table[3]=(dest_filter_assign_table[2]+spread) % NUM_FILTS;
			rot_dir[3]=spread_dir;
			while (				dest_filter_assign_table[3]==dest_filter_assign_table[0]
								|| dest_filter_assign_table[3]==dest_filter_assign_table[1]
								|| dest_filter_assign_table[3]==dest_filter_assign_table[2]
								|| dest_filter_assign_table[3]==lock_test5)
				dest_filter_assign_table[3]=(dest_filter_assign_table[3]+1) % NUM_FILTS;
		}


		//Assign [4] and check for duplicate placement over 0,1,2,3 & any locks
		if (lock[4]!=1){
			dest_filter_assign_table[4]=(dest_filter_assign_table[2]+(spread*2)) % NUM_FILTS;
			rot_dir[4]=spread_dir;
			while (				dest_filter_assign_table[4]==dest_filter_assign_table[0]
								|| dest_filter_assign_table[4]==dest_filter_assign_table[1]
								|| dest_filter_assign_table[4]==dest_filter_assign_table[2]
								|| dest_filter_assign_table[4]==dest_filter_assign_table[3]
								|| dest_filter_assign_table[4]==lock_test5)
				dest_filter_assign_table[4]=(dest_filter_assign_table[4]+1) % NUM_FILTS;
		}

		//Assign [5] and check for duplicate placement over 0,1,2,3,4
		if (lock[5]!=1) {
			dest_filter_assign_table[5]=(dest_filter_assign_table[2]+(spread*3)) % NUM_FILTS;
			rot_dir[5]=spread_dir;

			while (				dest_filter_assign_table[5]==dest_filter_assign_table[0]
			       				|| dest_filter_assign_table[5]==dest_filter_assign_table[1]
			       			    || dest_filter_assign_table[5]==dest_filter_assign_table[2]
			                    || dest_filter_assign_table[5]==dest_filter_assign_table[3]
			                    || dest_filter_assign_table[5]==dest_filter_assign_table[4]
								)
				dest_filter_assign_table[5]=(dest_filter_assign_table[5]+1) % NUM_FILTS;

		}

*/

	//	for (i=0;i<NUM_CHANNELS;i++){
	//		lock_test[i] = lock[i] ? dest_filter_assign_table[i] ? 99;
	//	}

		/*
		Try this:

		rot_dir[0]=-1.0*spread_dir;
		rot_dir[1]=-1.0*spread_dir;
		rot_dir[2]=-1.0*spread_dir;
		rot_dir[3]=spread_dir;
		rot_dir[4]=spread_dir;
		rot_dir[5]=spread_dir;

		spread_amt = spread - last_spread;

		if (!lock[0]) rotate_chan(0, -2 * spread_amt);
		if (!lock[1]) rotate_chan(1, -1 * spread_amt);
		//rotate_chan(2, 0 * spread_amt);
		if (!lock[3]) rotate_chan(3, 1 * spread_amt);
		if (!lock[4]) rotate_chan(4, 2 * spread_amt);
		if (!lock[5]) rotate_chan(5, 3 * spread_amt);




		void rotate_one(uint8_t channel, int8_t offset_amt){
			int8_t test_spot;
			int8_t test_scale;

				test_spot = (int8_t)dest_filter_assign_table[channel];
				test_spot = test_spot + offset_amt;

				test_scale = dest_scale[channel];


				do {	//repeat this block until is_distinct==1
					if (rotate_to_next_scale){

						if (test_spot>=NUMSCALES){
							while (test_spot >= NUMSCALES){
								test_spot-=NUMSCALES;
								test_scale++;
								if (test_scale>=NUMSCALES) test_scale=0;
							}
						} else if (test_spot<(-1*NUM_SCALES)) {

							while (test_spot < 0){
								test_spot += NUMSCALES;
								test_scale--;
								if (test_scale<0) test_scale=NUMSCALES-1;
							}
						}
					}

					for (is_distinct=1,j=0;j<NUM_CHANNELS;j++){
						//...if the test spot is already assigned to a locked channel, or a channel we already spread'ed, then try again
						if (i!=j && test_spot==dest_filter_assign_table[j] && (lock[j]==1 || (j<i)))
							is_distinct=0;
					}

					if (!is_distinct) test_spot+=rot_dir[channel];

				} while (!is_distinct)


				dest_filter_assign_table[channel] = test_spot;
				if (rotate_to_next_scale) dest_scale[channel] = test_scale;
		}


		*/



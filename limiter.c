/*
 * Idea for algoritm from https://www.kvraudio.com/forum/viewtopic.php?t=195315
 * Optimized for int32_t by Dan Green
 */

#include "globals.h"
#include "limiter.h"

float MAX_SAMPLEVAL_L;
float THRESHOLD_COMPILED;
float THRESHOLD_PERCENT;
int32_t THRESHOLD_VALUE;

void init_limiter(uint32_t max_sample_val, float threshold_percent)
{
	MAX_SAMPLEVAL_L=max_sample_val;
	THRESHOLD_PERCENT=threshold_percent;
	
	float m = (float)max_sample_val;
	THRESHOLD_COMPILED = m * m * threshold_percent * (1.0 - threshold_percent);

	THRESHOLD_VALUE = threshold_percent*max_sample_val;
}

int32_t limiter(int32_t val)
{
	float tv = THRESHOLD_COMPILED / ((float)val); // value to be subtracted for incoming signal going above theshold 
	if (val > THRESHOLD_VALUE) 		 return  (MAX_SAMPLEVAL_L - tv)  ;
	else if (val < -THRESHOLD_VALUE) return  (-MAX_SAMPLEVAL_L - tv) ; 
	else return val;
}
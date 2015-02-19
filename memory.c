/*
 * memory.c
 *
 *  Created on: Jun 8, 2014
 *      Author: design
 */
#include <misc.h>
#include "memory.h"
#include "inouts.h"
#include "globals.h"
#include "sdio.h"


//__IO uint16_t MEMORY[SPIRAM_END_ADDR];

extern uint8_t memfull;
extern uint32_t cur_rec_addr;
extern uint32_t g_error;


uint8_t write_memory(uint16_t *data, uint32_t addr){
	uint8_t err=0;
	//SD_WaitTransmissionEnd();

	err=SD_WriteSingleBlock((uint8_t *)data, addr);

	return(err);

}

uint8_t read_memory(uint16_t *data, uint32_t addr){
	uint8_t err=0;

	err=SD_ReadSingleBlock((uint8_t *)data, addr);

	return(err);
}



void init_memory(void){
	uint32_t i;

	SD_LowLevel_Init();
	SD_Init();

	test_memory();
}

void test_memory(void){
	uint16_t num_bytes=256;
	uint16_t test[256], test2[256];
	uint32_t i;
	uint32_t status_reg=0;


	for (i=0;i<num_bytes;i++){
		test[i]=(i<<6) + 17;
		test2[i]=0;
	}

	i=0;
	//i=180;
	//i=90;

	SD_ReadSingleBlock((uint8_t *)test2, i);
	SD_ReadSingleBlock((uint8_t *)test2, i+1);
	SD_ReadSingleBlock((uint8_t *)test2, i+2);
	SD_ReadSingleBlock((uint8_t *)test2, i+3);
	SD_ReadSingleBlock((uint8_t *)test2, i+4);
	SD_ReadSingleBlock((uint8_t *)test2, i+5);
	SD_ReadSingleBlock((uint8_t *)test2, i+6);
	SD_ReadSingleBlock((uint8_t *)test2, i+7);
	SD_ReadSingleBlock((uint8_t *)test2, i+8);
	SD_ReadSingleBlock((uint8_t *)test2, i+9);
	SD_ReadSingleBlock((uint8_t *)test2, i+10);
	SD_ReadSingleBlock((uint8_t *)test2, i+11);
	SD_ReadSingleBlock((uint8_t *)test2, i+12);
	SD_ReadSingleBlock((uint8_t *)test2, i+13);
	SD_ReadSingleBlock((uint8_t *)test2, i+14);
	SD_ReadSingleBlock((uint8_t *)test2, i+15);

	LED_ON(LED_BLUE);

	for (i=0;i<num_bytes;i++){
		if (test[i]!=test2[i])
			LED_OFF(LED_BLUE);

	}
}

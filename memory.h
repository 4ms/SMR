/*
* memory.h
 *
 *  Created on: Jun 8, 2014
 *      Author: design
 */

#ifndef MEMORY_H_
#define MEMORY_H_

/* PAGE_SIZE: number of 8-bit elements in a SDIO page */
#define PAGE_SIZE 512

/* BUFF_LEN: size of the mono 16-bit buffers. Should be PAGE_SIZE/2 since each element is 2x as big */
#define BUFF_LEN 256

/* STEREO_BUFF_LEN: number of elements in the buffer coming direct to/from the codec (not being used)*/
#define STEREO_BUFF_LEN 512

/* WRITE_BUFF_SIZE: number of elements in circular buffer array (each element has BUFF_LEN elements) */
#define WRITE_BUFF_SIZE 32

/* READ_BUFF_SIZE: number of elements in circular buffer array (each element has BUFF_LEN elements) */
#define READ_BUFF_SIZE 32

#define MAXSAMPLES 100
#define MEMORY_SIZE 0x00400000 /* 2GB / 512 Bytes per block = 4M blocks */


uint8_t read_memory(uint16_t *data, uint32_t addr);
uint8_t write_memory(uint16_t *data, uint32_t addr);
void init_memory(void);
void test_memory(void);

#endif /* MEMORY_H_ */

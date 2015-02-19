/*
 * sdio.h
 *
 *  Created on: Aug 12, 2014
 *      Author: design
 */

#ifndef SDIO_H_
#define SDIO_H_

void SD_LowLevel_Init(void);
void SD_Init(void);
uint8_t SD_WriteSingleBlock(uint8_t *buf, uint32_t blk);
uint8_t SD_ReadSingleBlock(uint8_t *buf, uint32_t blk);
//void SD_WaitReadWriteEnd(void);
uint8_t SD_WaitTransmissionEnd(void);
void SD_StartMultipleBlockWrite(uint32_t blk);
void SD_WriteData(uint8_t *buf, uint32_t cnt);
void SD_StopMultipleBlockWrite(void);


//PC7 is SD_CD

#endif /* SDIO_H_ */

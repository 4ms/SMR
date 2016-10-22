/*
 * i2s.c - I2S feeder routines
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

#include "i2s.h"
#include "dig_inouts.h"
#include "globals.h"

DMA_InitTypeDef dma_tx, dma_rx;
uint32_t txbuf, rxbuf;

volatile int16_t tx_buffer[codec_BUFF_LEN];
volatile int16_t rx_buffer[codec_BUFF_LEN];

DMA_InitTypeDef DMA_InitStructure, DMA_InitStructure2;
void I2S_Block_Init(void)
{
	NVIC_InitTypeDef nvic;


	/* Enable the DMA clock */
	RCC_AHB1PeriphClockCmd(AUDIO_I2S_DMA_CLOCK, ENABLE);

	/* Configure the TX DMA Stream */
	DMA_Cmd(AUDIO_I2S_DMA_STREAM, DISABLE);
	DMA_DeInit(AUDIO_I2S_DMA_STREAM);

	DMA_InitStructure.DMA_Channel = AUDIO_I2S_DMA_CHANNEL;
	DMA_InitStructure.DMA_PeripheralBaseAddr = AUDIO_I2S_DMA_DREG;
	DMA_InitStructure.DMA_Memory0BaseAddr = (uint32_t)0;      /* This field will be configured in play function */
	DMA_InitStructure.DMA_DIR = DMA_DIR_MemoryToPeripheral;
	DMA_InitStructure.DMA_BufferSize = (uint32_t)0xFFFE;      /* This field will be configured in play function */
	DMA_InitStructure.DMA_PeripheralInc = DMA_PeripheralInc_Disable;
	DMA_InitStructure.DMA_MemoryInc = DMA_MemoryInc_Enable;
	DMA_InitStructure.DMA_PeripheralDataSize = DMA_PeripheralDataSize_HalfWord;
	DMA_InitStructure.DMA_MemoryDataSize = DMA_MemoryDataSize_HalfWord;
	DMA_InitStructure.DMA_Mode = DMA_Mode_Circular;
	DMA_InitStructure.DMA_Priority = DMA_Priority_High;
	DMA_InitStructure.DMA_FIFOMode = DMA_FIFOMode_Disable;
	DMA_InitStructure.DMA_FIFOThreshold = DMA_FIFOThreshold_1QuarterFull;
	DMA_InitStructure.DMA_MemoryBurst = DMA_MemoryBurst_Single;
	DMA_InitStructure.DMA_PeripheralBurst = DMA_PeripheralBurst_Single;
	DMA_Init(AUDIO_I2S_DMA_STREAM, &DMA_InitStructure);

	/* Enable the I2S DMA request */
	SPI_I2S_DMACmd(CODEC_I2S, SPI_I2S_DMAReq_Tx, ENABLE);

	/* Configure the RX DMA Stream */
	DMA_Cmd(AUDIO_I2S_EXT_DMA_STREAM, DISABLE);
	DMA_DeInit(AUDIO_I2S_EXT_DMA_STREAM);

	DMA_InitStructure2.DMA_Channel = AUDIO_I2S_EXT_DMA_CHANNEL;
	DMA_InitStructure2.DMA_PeripheralBaseAddr = AUDIO_I2S_EXT_DMA_DREG;
	DMA_InitStructure2.DMA_Memory0BaseAddr = (uint32_t)0;      /* This field will be configured in play function */
	DMA_InitStructure2.DMA_DIR = DMA_DIR_PeripheralToMemory;
	DMA_InitStructure2.DMA_BufferSize = (uint32_t)0xFFFE;      /* This field will be configured in play function */
	DMA_InitStructure2.DMA_PeripheralInc = DMA_PeripheralInc_Disable;
	DMA_InitStructure2.DMA_MemoryInc = DMA_MemoryInc_Enable;
	DMA_InitStructure2.DMA_PeripheralDataSize = DMA_PeripheralDataSize_HalfWord;
	DMA_InitStructure2.DMA_MemoryDataSize = DMA_MemoryDataSize_HalfWord;
	DMA_InitStructure2.DMA_Mode = DMA_Mode_Circular;
	DMA_InitStructure2.DMA_Priority = DMA_Priority_High;
	DMA_InitStructure2.DMA_FIFOMode = DMA_FIFOMode_Disable;
	DMA_InitStructure2.DMA_FIFOThreshold = DMA_FIFOThreshold_1QuarterFull;
	DMA_InitStructure2.DMA_MemoryBurst = DMA_MemoryBurst_Single;
	DMA_InitStructure2.DMA_PeripheralBurst = DMA_PeripheralBurst_Single;
	DMA_Init(AUDIO_I2S_EXT_DMA_STREAM, &DMA_InitStructure2);

	/* Enable the Half & Complete DMA interrupts  */
	DMA_ITConfig(AUDIO_I2S_EXT_DMA_STREAM, DMA_IT_TC | DMA_IT_HT | DMA_IT_FE | DMA_IT_TE | DMA_IT_DME, ENABLE);


	/* I2S RX DMA IRQ Channel configuration */

	nvic.NVIC_IRQChannel = AUDIO_I2S_EXT_DMA_IRQ;
	nvic.NVIC_IRQChannelPreemptionPriority = 0;
	nvic.NVIC_IRQChannelSubPriority = 0;
	nvic.NVIC_IRQChannelCmd = ENABLE;

	NVIC_Init(&nvic);

//	NVIC_DisableIRQ(AUDIO_I2S_EXT_DMA_IRQ);

	/* I2S DMA IRQ Channel configuration */
	NVIC_EnableIRQ(AUDIO_I2S_EXT_DMA_IRQ);

	/* Enable the I2S DMA request */
	SPI_I2S_DMACmd(CODEC_I2S_EXT, SPI_I2S_DMAReq_Rx, ENABLE);

}

void I2S_Block_PlayRec(void)
{
	uint32_t i;
	uint32_t Size = codec_BUFF_LEN;

	/* save for IRQ svc  */
	txbuf = (uint32_t)&tx_buffer;
	rxbuf = (uint32_t)&rx_buffer;

	/* Configure the tx buffer address and size */
	DMA_InitStructure.DMA_Memory0BaseAddr = (uint32_t)&tx_buffer;
	DMA_InitStructure.DMA_BufferSize = (uint32_t)Size;

	/* Configure the DMA Stream with the new parameters */
	DMA_Init(AUDIO_I2S_DMA_STREAM, &DMA_InitStructure);

	/* Configure the rx buffer address and size */
	/* Again with the separate initstructure. Baroo?? */
	DMA_InitStructure2.DMA_Memory0BaseAddr = (uint32_t)&rx_buffer;
	DMA_InitStructure2.DMA_BufferSize = (uint32_t)Size;

	/* Configure the DMA Stream with the new parameters */
	DMA_Init(AUDIO_I2S_EXT_DMA_STREAM, &DMA_InitStructure2);

	/* Enable the I2S DMA Streams */
	DMA_Cmd(AUDIO_I2S_DMA_STREAM, ENABLE);
	DMA_Cmd(AUDIO_I2S_EXT_DMA_STREAM, ENABLE);

	/* If the I2S peripheral is still not enabled, enable it */
	if ((CODEC_I2S->I2SCFGR & 0x0400) == 0)
	{
		I2S_Cmd(CODEC_I2S, ENABLE);
	}
	if ((CODEC_I2S_EXT->I2SCFGR & 0x0400) == 0)
	{
		I2S_Cmd(CODEC_I2S_EXT, ENABLE);
	}
}


void DMA1_Stream3_IRQHandler(void)
{
	int16_t *src, *dst, sz;
	uint8_t i;

	if (DMA_GetFlagStatus(AUDIO_I2S_EXT_DMA_STREAM, AUDIO_I2S_EXT_DMA_FLAG_FE) != RESET)
		i=0;

	if (DMA_GetFlagStatus(AUDIO_I2S_EXT_DMA_STREAM, AUDIO_I2S_EXT_DMA_FLAG_TE) != RESET)
		i=0;

	if (DMA_GetFlagStatus(AUDIO_I2S_EXT_DMA_STREAM, AUDIO_I2S_EXT_DMA_FLAG_DME) != RESET)
		i=0;

	// Transfer complete interrupt
	if (DMA_GetFlagStatus(AUDIO_I2S_EXT_DMA_STREAM, AUDIO_I2S_EXT_DMA_FLAG_TC) != RESET)
	{
		sz = codec_BUFF_LEN/2;
		src = (int16_t *)(rxbuf) + sz;
		dst = (int16_t *)(txbuf) + sz;

		process_audio_block(src, dst, 0);

		DMA_ClearFlag(AUDIO_I2S_EXT_DMA_STREAM, AUDIO_I2S_EXT_DMA_FLAG_TC);
	}

	// Half Transfer complete interrupt
	if (DMA_GetFlagStatus(AUDIO_I2S_EXT_DMA_STREAM, AUDIO_I2S_EXT_DMA_FLAG_HT) != RESET)
	{
		sz = codec_BUFF_LEN/2;
		src = (int16_t *)(rxbuf);
		dst = (int16_t *)(txbuf);

		process_audio_block(src, dst, 1);

		DMA_ClearFlag(AUDIO_I2S_EXT_DMA_STREAM, AUDIO_I2S_EXT_DMA_FLAG_HT);
	}

}







/*
 * adc.c - adc setup
 */
 
#include "adc.h"
#include "inouts.h"


void ADC1_Init(uint16_t *ADC_Buffer)
{
	DMA_InitTypeDef DMA_InitStructure;
	ADC_CommonInitTypeDef ADC_CommonInitStructure;
	ADC_InitTypeDef ADC_InitStructure;
	GPIO_InitTypeDef GPIO_InitStructure;

	/* enable clocks for DMA2, ADC1, GPIOs ----------------------------------*/
	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_DMA2 | RCC_AHB1Periph_GPIOA | RCC_AHB1Periph_GPIOC | RCC_AHB1Periph_GPIOB, ENABLE);
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_ADC1, ENABLE);


	/* DMA2 stream0 channel0 configuration ----------------------------------*/
	DMA_InitStructure.DMA_Channel = DMA_Channel_0;  
	DMA_InitStructure.DMA_PeripheralBaseAddr = (uint32_t)&ADC1->DR;
	DMA_InitStructure.DMA_Memory0BaseAddr = (uint32_t)ADC_Buffer;
	DMA_InitStructure.DMA_DIR = DMA_DIR_PeripheralToMemory;
	DMA_InitStructure.DMA_BufferSize = NUM_ADCS;
	DMA_InitStructure.DMA_PeripheralInc = DMA_PeripheralInc_Disable;
	DMA_InitStructure.DMA_MemoryInc = DMA_MemoryInc_Enable;
	DMA_InitStructure.DMA_PeripheralDataSize = DMA_PeripheralDataSize_HalfWord;
	DMA_InitStructure.DMA_MemoryDataSize = DMA_MemoryDataSize_HalfWord;
	DMA_InitStructure.DMA_Mode = DMA_Mode_Circular;
	DMA_InitStructure.DMA_Priority = DMA_Priority_High;
	DMA_InitStructure.DMA_FIFOMode = DMA_FIFOMode_Disable;         
	DMA_InitStructure.DMA_FIFOThreshold = DMA_FIFOThreshold_HalfFull;
	DMA_InitStructure.DMA_MemoryBurst = DMA_MemoryBurst_Single;
	DMA_InitStructure.DMA_PeripheralBurst = DMA_PeripheralBurst_Single;
	DMA_Init(DMA2_Stream0, &DMA_InitStructure);
	DMA_Cmd(DMA2_Stream0, ENABLE);

	/* ADC Common Init ------------------------------------------------------*/
	ADC_CommonInitStructure.ADC_Mode = ADC_Mode_Independent;
	ADC_CommonInitStructure.ADC_Prescaler = ADC_Prescaler_Div8;
	ADC_CommonInitStructure.ADC_DMAAccessMode = ADC_DMAAccessMode_Disabled;
	ADC_CommonInitStructure.ADC_TwoSamplingDelay = ADC_TwoSamplingDelay_5Cycles;
	ADC_CommonInit(&ADC_CommonInitStructure);

	/* ADC1 Init ------------------------------------------------------------*/
	ADC_InitStructure.ADC_Resolution = ADC_Resolution_12b;
	ADC_InitStructure.ADC_ScanConvMode = ENABLE;
	ADC_InitStructure.ADC_ContinuousConvMode = ENABLE;
	ADC_InitStructure.ADC_ExternalTrigConvEdge = ADC_ExternalTrigConvEdge_None;
	ADC_InitStructure.ADC_ExternalTrigConv = ADC_ExternalTrigConv_T1_CC1;
	ADC_InitStructure.ADC_DataAlign = ADC_DataAlign_Right;
	ADC_InitStructure.ADC_NbrOfConversion = NUM_ADCS;
	ADC_Init(ADC1, &ADC_InitStructure);
	
	/* Configure analog input pins ------------------------------------------*/
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AN;
	GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL ;

	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_7 | GPIO_Pin_6 | GPIO_Pin_5 | GPIO_Pin_3 | GPIO_Pin_2 | GPIO_Pin_1 | GPIO_Pin_0; //Channel 7, 6, 5, 3, 2, 1, 0
	GPIO_Init(GPIOA, &GPIO_InitStructure);
	
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_5 | GPIO_Pin_4 | GPIO_Pin_3 | GPIO_Pin_2 | GPIO_Pin_1 | GPIO_Pin_0; //Channel 15, 14, 13, 12, 11, 10
	GPIO_Init(GPIOC, &GPIO_InitStructure);

	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0 | GPIO_Pin_1 ; //Channel 8, 9
	GPIO_Init(GPIOB, &GPIO_InitStructure);

	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_6; //Channel 4
	GPIO_Init(GPIOF, &GPIO_InitStructure);

	/* ADC1 regular channel configuration -----------------------------------*/ 
	ADC_RegularChannelConfig(ADC1, ADC_Channel_0, MORPH_ADC+1, ADC_SampleTime_144Cycles); //[0]: PA0, Morph
	ADC_RegularChannelConfig(ADC1, ADC_Channel_1, QVAL_ADC+1, ADC_SampleTime_144Cycles); //[1]: PA1, Q CV
	ADC_RegularChannelConfig(ADC1, ADC_Channel_2, LEVEL_ADC_BASE+1, ADC_SampleTime_144Cycles); //[2]: PA2: filter 1
	ADC_RegularChannelConfig(ADC1, ADC_Channel_3, LEVEL_ADC_BASE+2, ADC_SampleTime_144Cycles); //[3]: PA3: filter 2
	ADC_RegularChannelConfig(ADC1, ADC_Channel_6, LEVEL_ADC_BASE+3, ADC_SampleTime_144Cycles); //[4]: PA6: filter 3
	ADC_RegularChannelConfig(ADC1, ADC_Channel_7, LEVEL_ADC_BASE+4, ADC_SampleTime_144Cycles); //[5]: PA7: filter 4
	ADC_RegularChannelConfig(ADC1, ADC_Channel_10, LEVEL_ADC_BASE+5, ADC_SampleTime_144Cycles); //[6]: PC0: filter 5
	ADC_RegularChannelConfig(ADC1, ADC_Channel_11, LEVEL_ADC_BASE+6, ADC_SampleTime_144Cycles); //[7]: PC1: filter 6
	ADC_RegularChannelConfig(ADC1, ADC_Channel_12, FREQNUDGE1_ADC+1, ADC_SampleTime_144Cycles); //[8]: PC2: freq 1
	ADC_RegularChannelConfig(ADC1, ADC_Channel_13, FREQNUDGE6_ADC+1, ADC_SampleTime_144Cycles);//[9]: PC3: freq 6
	ADC_RegularChannelConfig(ADC1, ADC_Channel_14, SCALE_ADC+1, ADC_SampleTime_144Cycles); //[10]: PC4: Scale CV
	ADC_RegularChannelConfig(ADC1, ADC_Channel_15, SPREAD_ADC+1, ADC_SampleTime_144Cycles); //[11]: PC5: spread CV
	ADC_RegularChannelConfig(ADC1, ADC_Channel_5, ROTCV_ADC+1, ADC_SampleTime_144Cycles);//[12]: PA5: Rot CV/Reset
	ADC_RegularChannelConfig(ADC1, ADC_Channel_8, FM_135_ADC+1, ADC_SampleTime_144Cycles);//[13]: PB0: FM_135
	ADC_RegularChannelConfig(ADC1, ADC_Channel_9, FM_246_ADC+1, ADC_SampleTime_144Cycles);//[14]: PB1: FM_246


	/* Enable Complete DMA interrupt  */
	//DMA_ITConfig(DMA2_Stream0, DMA_IT_TC, ENABLE);
    
	/* ADC DMA IRQ Channel configuration */
	//NVIC_EnableIRQ(DMA2_Stream0_IRQn);
	
	/* Enable DMA request after last transfer (Single-ADC mode) */
	ADC_DMARequestAfterLastTransferCmd(ADC1, ENABLE);
	
	/* Enable ADC1 DMA */
	ADC_DMACmd(ADC1, ENABLE);

	/* Enable ADC1 */
	ADC_Cmd(ADC1, ENABLE);
	
	/* Start ADC1 Software Conversion */ 
	ADC_SoftwareStartConv(ADC1);

}

void ADC3_Init(void){
	ADC_CommonInitTypeDef ADC_CommonInitStructure;
	ADC_InitTypeDef ADC_InitStructure;
	GPIO_InitTypeDef GPIO_InitStructure;

	RCC_APB2PeriphClockCmd(RCC_APB2Periph_ADC3, ENABLE);
	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOF, ENABLE);

	/* ADC Common Init ------------------------------------------------------*/
	ADC_CommonInitStructure.ADC_Mode = ADC_Mode_Independent;
	ADC_CommonInitStructure.ADC_Prescaler = ADC_Prescaler_Div8;
	ADC_CommonInitStructure.ADC_DMAAccessMode = ADC_DMAAccessMode_Disabled;
	ADC_CommonInitStructure.ADC_TwoSamplingDelay = ADC_TwoSamplingDelay_5Cycles;
	ADC_CommonInit(&ADC_CommonInitStructure);

	/* ADC3 Init ------------------------------------------------------------*/
	ADC_InitStructure.ADC_Resolution = ADC_Resolution_12b;
	ADC_InitStructure.ADC_ScanConvMode = DISABLE;
	ADC_InitStructure.ADC_ContinuousConvMode = ENABLE;
	ADC_InitStructure.ADC_ExternalTrigConvEdge = ADC_ExternalTrigConvEdge_None;
	ADC_InitStructure.ADC_ExternalTrigConv = ADC_ExternalTrigConv_T1_CC1;
	ADC_InitStructure.ADC_DataAlign = ADC_DataAlign_Right;
	ADC_InitStructure.ADC_NbrOfConversion = 1;
	ADC_Init(ADC3, &ADC_InitStructure);


	/* Configure analog input pins ------------------------------------------*/
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AN;
	GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL ;
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_6; //Channel 4
	GPIO_Init(GPIOF, &GPIO_InitStructure);

	/* ADC1 regular channel configuration -----------------------------------*/
	ADC_RegularChannelConfig(ADC3, ADC_Channel_4, 1, ADC_SampleTime_480Cycles); //[0]: PF6: Q Pot

	/* Enable ADC1 */
	ADC_Cmd(ADC3, ENABLE);

	ADC_SoftwareStartConv(ADC3);
}

uint32_t check_ADC3(void){
	uint32_t t=ADC3->DR;

	ADC_SoftwareStartConv(ADC3);

	return(t);

}

void DMA2_Stream0_IRQHandler(void)
{ 
	//This takes about 800ns - 950ns, and there's as little as 1000ns between occurances

	/* Transfer complete interrupt */
	if (DMA_GetFlagStatus(DMA2_Stream0, DMA_FLAG_TCIF0) != RESET)
	{
		/* Clear the Interrupt flag */
		DMA_ClearFlag(DMA2_Stream0, DMA_FLAG_TCIF0);
		
		/* Start ADC1 Software Conversion */ 
		//ADC_SoftwareStartConv(ADC1);
	}

}

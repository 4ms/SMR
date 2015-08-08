/*
 * codec.c - Discovery board codec interface routines
 *
 * Cut from stm32f4_discovery_audio_codec.c
 *
 */

/*========================

                CS43L22 Audio Codec Control Functions
                                                ==============================*/
/**
  * @brief  Initializes the audio codec and all related interfaces (control 
  *         interface: I2C and audio interface: I2S)
  * @param  Volume: Initial volume level (from 0 (Mute) to 100 (Max))
  * @param  AudioFreq: Audio frequency used to play the audio stream.
  * @retval 0 if correct communication, else wrong communication
  */
  
#include "codec.h"

/* Mask for the bit EN of the I2S CFGR register */
#define I2S_ENABLE_MASK                 0x0400

/* Codec audio Standards */
#ifdef I2S_STANDARD_PHILLIPS
 #define  CODEC_STANDARD                0x04
 #define I2S_STANDARD                   I2S_Standard_Phillips         
#elif defined(I2S_STANDARD_MSB)
 #define  CODEC_STANDARD                0x00
 #define I2S_STANDARD                   I2S_Standard_MSB    
#elif defined(I2S_STANDARD_LSB)
 #define  CODEC_STANDARD                0x08
 #define I2S_STANDARD                   I2S_Standard_LSB    
#else 
 #error "Error: No audio communication standard selected !"
#endif /* I2S_STANDARD */

#define W8731_ADDR_0 0x1A
#define W8731_ADDR_1 0x1B
#define W8731_NUM_REGS 10

#define INBOTH 8
#define INMUTE 7
#define INVOL 0

#define VOL_p12dB	0b11111 /*+12dB*/
#define VOL_0dB		0b10111 /*0dB*/
#define VOL_n1dB	0b10110 /*-1.5dB*/
#define VOL_n3dB	0b10101 /*-3dB*/
#define VOL_n6dB	0b10011 /*-6dB*/
#define VOL_n12dB	15 		/*-12dB*/
#define VOL_n15dB	13 		/*-15dB*/
/*1.5dB steps down to..*/
#define VOL_n34dB	0b00000 /*-34.5dB*/

//Register 4: Analogue Audio Path Control
#define MICBOOST 		(1 << 0)	/* Boost Mic level */
#define MUTEMIC			(1 << 1)	/* Mute Mic to ADC */
#define INSEL_mic		(1 << 2)	/* Mic Select*/
#define INSEL_line		(0 << 2)	/* LineIn Select*/
#define BYPASS			(1 << 3)	/* Bypass Enable */
#define DACSEL			(1 << 4)	/* Select DAC */
#define SIDETONE		(1 << 5)	/* Enable Sidetone */
#define SIDEATT_neg15dB	(0b11 << 6)
#define SIDEATT_neg12dB	(0b10 << 6)
#define SIDEATT_neg9dB	(0b01 << 6)
#define SIDEATT_neg6dB	(0b00 << 6)


//Register 5: Digital Audio Path Control
#define ADCHPFDisable 1 				/* ADC High Pass Filter */
#define ADCHPFEnable 0
#define DEEMPH_48k		(0b11 << 1) 	/* De-emphasis Control */
#define DEEMPH_44k		(0b10 << 1)
#define DEEMPH_32k 		(0b01 << 1)
#define DEEMPH_disable	(0b00 << 1)
#define DACMU_enable	(1 << 3) 		/* DAC Soft Mute Control */
#define DACMU_disable	(0 << 3)
#define HPOR_store		(1 << 4) 		/* Store DC offset when HPF disabled */
#define HPOR_clear		(0 << 4)

//Register 7: Digital Audio Interface Format
#define format_MSB_Right 0
#define format_MSB_Left 1
#define format_I2S 2
#define format_DSP 3
#define format_16b (0<<2)
#define format_20b (1<<2)
#define format_24b (2<<2)
#define format_32b (3<<2)

// Oddness:
// format_I2S does not work with I2S2 on the STM32F427Z (works on the 427V) in Master TX mode (I2S2ext is RX)
// The RX data is shifted left 2 bits (x4) as it comes in, causing digital wrap-around clipping.
// Using format_MSB_Left works (I2S periph has to be set up I2S_Standard_LSB or I2S_Standard_MSB).
// Also, format_MSB_Right does not seem to work at all (with the I2S set to LSB or MSB)

const uint16_t w8731_init_data[] =
{
	VOL_n6dB,			// Reg 00: Left Line In

	VOL_n6dB,			// Reg 01: Right Line In

	0b0101111,			// Reg 02: Left Headphone out (Mute)

	0b0101111,			// Reg 03: Right Headphone out (Mute)

	(MUTEMIC 			// Reg 04: Analog Audio Path Control (maximum attenuation on sidetone, sidetone disabled, DAC selected, Mute Mic, no bypass)
	| INSEL_line
	| DACSEL
	| SIDEATT_neg6dB),

	(DEEMPH_48k			// Reg 05: Digital Audio Path Control: HPF, De-emp at 48kHz on DAC, do not soft mute dac
	| ADCHPFEnable),

	0x062,				// Reg 06: Power Down Control (Clkout, Osc, Mic Off)

	(format_24b			// Reg 07: Digital Audio Interface Format (24-bit, slave)
	| format_I2S),

	0x000,				// Reg 08: Sampling Control (Normal, 256x, 48k ADC/DAC)

	0x001				// Reg 09: Active Control
};



/*
const uint16_t w8731_init_data[] = 
{
	0x017,			// Reg 00: Left Line In (0dB, mute off)
	0x017,			// Reg 01: Right Line In (0dB, mute off)
	0b0101111,			// Reg 02: Left Headphone out (Mute)
	0b0101111,			// Reg 03: Right Headphone out (Mute)
	0x012,			// Reg 04: Analog Audio Path Control (DAC sel, Mute Mic)
	0x000,			// Reg 05: Digital Audio Path Control
	0x062,			// Reg 06: Power Down Control (Clkout, Osc, Mic Off)
	0x00A,			// Reg 07: Digital Audio Interface Format (i2s, 24-bit, slave)
	0x000,			// Reg 08: Sampling Control (Normal, 256x, 48k ADC/DAC)
	0x001			// Reg 09: Active Control
};*/

/* The 7 bits Codec address (sent through I2C interface) */
#define CODEC_ADDRESS           (W8731_ADDR_0<<1)

/* local vars */
__IO uint32_t  CODECTimeout = CODEC_LONG_TIMEOUT;   
__IO uint8_t OutputDev = 0;


/**
  * @brief  Inserts a delay time (not accurate timing).
  * @param  nCount: specifies the delay time length.
  * @retval None
  */
static void Delay( __IO uint32_t nCount)
{	for (; nCount != 0; nCount--);
}

#ifdef USE_DEFAULT_TIMEOUT_CALLBACK
/**
  * @brief  Basic management of the timeout situation.
  * @param  None
  * @retval None
  */
uint32_t Codec_TIMEOUT_UserCallback(void)
{
	/* Block communication and all processes */
	while (1)
	{   
	}
}
#else
uint32_t Codec_TIMEOUT_UserCallback(void)
{
	/* nothing */
	return 1;
}
#endif /* USE_DEFAULT_TIMEOUT_CALLBACK */

uint32_t Codec_Init(uint32_t AudioFreq)
{
	uint32_t err = 0;

	/* Configure the Codec related IOs */
	Codec_GPIO_Init();   

	/* Initialize the Control interface of the Audio Codec */
	Codec_CtrlInterface_Init();     

	/* Configure the I2S peripheral */
	Codec_AudioInterface_Init(AudioFreq);  

	/* Reset the Codec Registers */
	err=Codec_Reset();

	/* Return communication control value */
	return err;
}

/**
  * @brief  Resets the audio codec. It restores the default configuration of the 
  *         codec (this function shall be called before initializing the codec).
  * @note   This function calls an external driver function: The IO Expander driver.
  * @param  None
  * @retval None
  */
uint32_t Codec_Reset(void)
{
	uint8_t i;
	uint32_t err=0;
	
	err=Codec_WriteRegister(0x0f, 0);
	
	/* Load default values */
	for(i=0;i<W8731_NUM_REGS;i++)
	{
		err=Codec_WriteRegister(i, w8731_init_data[i]);
	}
	return err;
}

/**
  * @brief  Writes a Byte to a given register into the audio codec through the 
            control interface (I2C)
  * @param  RegisterAddr: The address (location) of the register to be written.
  * @param  RegisterValue: the Byte value to be written into destination register.
  * @retval 0 if correct communication, else wrong communication
  */
uint32_t Codec_WriteRegister(uint8_t RegisterAddr, uint16_t RegisterValue)
{
	uint32_t result = 0;
	
	/* Assemble 2-byte data in WM8731 format */
	uint8_t Byte1 = ((RegisterAddr<<1)&0xFE) | ((RegisterValue>>8)&0x01);
	uint8_t Byte2 = RegisterValue&0xFF;
	
	/*!< While the bus is busy */
	CODECTimeout = CODEC_LONG_TIMEOUT;
	while(I2C_GetFlagStatus(CODEC_I2C, I2C_FLAG_BUSY))
	{
		if((CODECTimeout--) == 0)
			return Codec_TIMEOUT_UserCallback();
	}

	/* Start the config sequence */
	I2C_GenerateSTART(CODEC_I2C, ENABLE);

	/* Test on EV5 and clear it */
	CODECTimeout = CODEC_FLAG_TIMEOUT;
	while (!I2C_CheckEvent(CODEC_I2C, I2C_EVENT_MASTER_MODE_SELECT))
	{
		if((CODECTimeout--) == 0)
			return Codec_TIMEOUT_UserCallback();
	}

	/* Transmit the slave address and enable writing operation */
	I2C_Send7bitAddress(CODEC_I2C, CODEC_ADDRESS, I2C_Direction_Transmitter);

	/* Test on EV6 and clear it */
	CODECTimeout = CODEC_FLAG_TIMEOUT;
	while (!I2C_CheckEvent(CODEC_I2C, I2C_EVENT_MASTER_TRANSMITTER_MODE_SELECTED))
	{
		if((CODECTimeout--) == 0)
			return Codec_TIMEOUT_UserCallback();
	}

	/* Transmit the first address for write operation */
	I2C_SendData(CODEC_I2C, Byte1);

	/* Test on EV8 and clear it */
	CODECTimeout = CODEC_FLAG_TIMEOUT;
	while (!I2C_CheckEvent(CODEC_I2C, I2C_EVENT_MASTER_BYTE_TRANSMITTING))
	{
		if((CODECTimeout--) == 0)
			return Codec_TIMEOUT_UserCallback();
	}

	/* Prepare the register value to be sent */
	I2C_SendData(CODEC_I2C, Byte2);

	/*!< Wait till all data have been physically transferred on the bus */
	CODECTimeout = CODEC_LONG_TIMEOUT;
	while(!I2C_GetFlagStatus(CODEC_I2C, I2C_FLAG_BTF))
	{
		if((CODECTimeout--) == 0)
			return Codec_TIMEOUT_UserCallback();
	}

	/* End the configuration sequence */
	I2C_GenerateSTOP(CODEC_I2C, ENABLE);  

	/* Return the verifying value: 0 (Passed) or 1 (Failed) */
	return result;  
}

/**
  * @brief  Initializes the Audio Codec control interface (I2C).
  * @param  None
  * @retval None
  */
void Codec_CtrlInterface_Init(void)
{
	I2C_InitTypeDef I2C_InitStructure;

	/* Enable the CODEC_I2C peripheral clock */
	RCC_APB1PeriphClockCmd(CODEC_I2C_CLK, ENABLE);

	/* CODEC_I2C peripheral configuration */
	I2C_DeInit(CODEC_I2C);
	I2C_InitStructure.I2C_Mode = I2C_Mode_I2C;
	I2C_InitStructure.I2C_DutyCycle = I2C_DutyCycle_2;
	I2C_InitStructure.I2C_OwnAddress1 = 0x33;
	I2C_InitStructure.I2C_Ack = I2C_Ack_Enable;
	I2C_InitStructure.I2C_AcknowledgedAddress = I2C_AcknowledgedAddress_7bit;
	I2C_InitStructure.I2C_ClockSpeed = I2C_SPEED;
	
	/* Enable the I2C peripheral */
	I2C_Cmd(CODEC_I2C, ENABLE);  
	I2C_Init(CODEC_I2C, &I2C_InitStructure);
}

/**
  * @brief  Initializes the Audio Codec audio interface (I2S)
  * @note   This function assumes that the I2S input clock (through PLL_R in 
  *         Devices RevA/Z and through dedicated PLLI2S_R in Devices RevB/Y)
  *         is already configured and ready to be used.    
  * @param  AudioFreq: Audio frequency to be configured for the I2S peripheral. 
  * @retval None
  */
void Codec_AudioInterface_Init(uint32_t AudioFreq)
{
	I2S_InitTypeDef I2S_InitStructure;

	/* Enable the CODEC_I2S peripheral clock */
	RCC_APB1PeriphClockCmd(CODEC_I2S_CLK, ENABLE);

	/* CODEC_I2S peripheral configuration for master TX */
	SPI_I2S_DeInit(CODEC_I2S);
	I2S_InitStructure.I2S_AudioFreq = AudioFreq;
	I2S_InitStructure.I2S_Standard = I2S_STANDARD;
	I2S_InitStructure.I2S_DataFormat = I2S_DataFormat_24b;//extended;
	I2S_InitStructure.I2S_CPOL = I2S_CPOL_Low;
	I2S_InitStructure.I2S_Mode = I2S_Mode_MasterTx;
	I2S_InitStructure.I2S_MCLKOutput = I2S_MCLKOutput_Enable;

	/* Initialize the I2S main channel for TX */
	I2S_Init(CODEC_I2S, &I2S_InitStructure);
	
	/* Initialize the I2S extended channel for RX */
	I2S_FullDuplexConfig(CODEC_I2S_EXT, &I2S_InitStructure);

	/* The I2S peripheral will be enabled only in the EVAL_AUDIO_Play() function 
	or by user functions if DMA mode not enabled */  
}

/**
  * @brief Initializes IOs used by the Audio Codec (on the control and audio 
  *        interfaces).
  * @param  None
  * @retval None
  */
void Codec_GPIO_Init(void)
{
	GPIO_InitTypeDef GPIO_InitStructure;

	/* Enable I2S and I2C GPIO clocks */
	RCC_AHB1PeriphClockCmd(CODEC_I2C_GPIO_CLOCK | CODEC_I2S_GPIO_CLOCK, ENABLE);

	/* CODEC_I2C SCL and SDA pins configuration -------------------------------------*/
	GPIO_InitStructure.GPIO_Pin = CODEC_I2C_SCL_PIN | CODEC_I2C_SDA_PIN; 
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_2MHz;
	GPIO_InitStructure.GPIO_OType = GPIO_OType_OD;
	GPIO_InitStructure.GPIO_PuPd  = GPIO_PuPd_NOPULL;
	GPIO_Init(CODEC_I2C_GPIO, &GPIO_InitStructure);

	/* Connect pins to I2C peripheral */
	GPIO_PinAFConfig(CODEC_I2C_GPIO, CODEC_I2S_SCL_PINSRC, CODEC_I2C_GPIO_AF);  
	GPIO_PinAFConfig(CODEC_I2C_GPIO, CODEC_I2S_SDA_PINSRC, CODEC_I2C_GPIO_AF);  

	/* CODEC_I2S output pins configuration: WS, SCK SD0 and SDI pins ------------------*/
	GPIO_InitStructure.GPIO_Pin = CODEC_I2S_SCK_PIN | CODEC_I2S_SDO_PIN | CODEC_I2S_SDI_PIN | CODEC_I2S_WS_PIN; 
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_2MHz;
	GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
	GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL;
	GPIO_Init(CODEC_I2S_GPIO, &GPIO_InitStructure);

	/* CODEC_I2S pins configuration: MCK pin */
	GPIO_InitStructure.GPIO_Pin = CODEC_I2S_MCK_PIN; 
	GPIO_Init(CODEC_I2S_MCK_GPIO, &GPIO_InitStructure);

	/* Connect pins to I2S peripheral  */
	GPIO_PinAFConfig(CODEC_I2S_GPIO, CODEC_I2S_WS_PINSRC, CODEC_I2S_GPIO_AF);  
	GPIO_PinAFConfig(CODEC_I2S_GPIO, CODEC_I2S_SCK_PINSRC, CODEC_I2S_GPIO_AF);
	GPIO_PinAFConfig(CODEC_I2S_GPIO, CODEC_I2S_SDO_PINSRC, CODEC_I2S_GPIO_AF);
	GPIO_PinAFConfig(CODEC_I2S_GPIO, CODEC_I2S_SDI_PINSRC, CODEC_I2S_GPIO_AF);
	GPIO_PinAFConfig(CODEC_I2S_MCK_GPIO, CODEC_I2S_MCK_PINSRC, CODEC_I2S_GPIO_AF); 
}

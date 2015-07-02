/*
 * led_driver.c
 *
 *  Created on: Nov 30, 2014
 *      Author: design
 */
#include "stm32f4xx.h"
#include "stm32f4xx_gpio.h"
#include "stm32f4xx_i2c.h"
#include "stm32f4xx_spi.h"
#include "stm32f4xx_rcc.h"
#include "inouts.h"
#include "led_driver.h"

#define delay(x)						\
do {							\
  register unsigned int i;				\
  for (i = 0; i < x; ++i)				\
    __asm__ __volatile__ ("nop\n\t":::"memory");	\
} while (0)


__IO uint32_t  LEDDriverTimeout = LEDDRIVER_LONG_TIMEOUT;

void LEDDriver_GPIO_Init(void)
{
	GPIO_InitTypeDef GPIO_InitStructure;

	/* Enable I2S and I2C GPIO clocks */
	RCC_AHB1PeriphClockCmd(LEDDRIVER_I2C_GPIO_CLOCK, ENABLE);

	/* LEDDRIVER_I2C SCL and SDA pins configuration -------------------------------------*/
	GPIO_InitStructure.GPIO_Pin = LEDDRIVER_I2C_SCL_PIN | LEDDRIVER_I2C_SDA_PIN;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_InitStructure.GPIO_OType = GPIO_OType_OD;
	GPIO_InitStructure.GPIO_PuPd  = GPIO_PuPd_NOPULL;
	GPIO_Init(LEDDRIVER_I2C_GPIO, &GPIO_InitStructure);

	/* Connect pins to I2C peripheral */
	GPIO_PinAFConfig(LEDDRIVER_I2C_GPIO, LEDDRIVER_I2S_SCL_PINSRC, LEDDRIVER_I2C_GPIO_AF);
	GPIO_PinAFConfig(LEDDRIVER_I2C_GPIO, LEDDRIVER_I2S_SDA_PINSRC, LEDDRIVER_I2C_GPIO_AF);

}

void LEDDriver_I2C_Init(void)
{
	I2C_InitTypeDef I2C_InitStructure;

	/* Enable the LEDDRIVER_I2C peripheral clock */
	RCC_APB1PeriphClockCmd(LEDDRIVER_I2C_CLK, ENABLE);

	/* LEDDRIVER_I2C peripheral configuration */
	I2C_DeInit(LEDDRIVER_I2C);
	I2C_InitStructure.I2C_Mode = I2C_Mode_I2C;
	I2C_InitStructure.I2C_DutyCycle = I2C_DutyCycle_2;
	I2C_InitStructure.I2C_OwnAddress1 = 0x34;
	I2C_InitStructure.I2C_Ack = I2C_Ack_Enable;
	I2C_InitStructure.I2C_AcknowledgedAddress = I2C_AcknowledgedAddress_7bit;
	I2C_InitStructure.I2C_ClockSpeed = I2C1_SPEED;

	/* Enable the I2C peripheral */
	I2C_Cmd(LEDDRIVER_I2C, ENABLE);
	I2C_Init(LEDDRIVER_I2C, &I2C_InitStructure);
}


uint32_t LEDDRIVER_TIMEOUT_UserCallback(void)
{
	/* nothing */
	return 1;
}

uint32_t LEDDriver_writeregister(uint8_t driverAddr, uint8_t RegisterAddr, uint8_t RegisterValue){
	uint32_t result = 0;

	driverAddr = PCA9685_I2C_BASE_ADDRESS | (driverAddr << 1);
	//driverAddr = 0b10000000;

	/*!< While the bus is busy */
	LEDDriverTimeout = LEDDRIVER_LONG_TIMEOUT;
	while(I2C_GetFlagStatus(LEDDRIVER_I2C, I2C_FLAG_BUSY))
	{
		if((LEDDriverTimeout--) == 0)
			return LEDDRIVER_TIMEOUT_UserCallback();
	}

	/* Start the config sequence */
	I2C_GenerateSTART(LEDDRIVER_I2C, ENABLE);

	LEDDriverTimeout = LEDDRIVER_FLAG_TIMEOUT;
	while (!I2C_CheckEvent(LEDDRIVER_I2C, I2C_EVENT_MASTER_MODE_SELECT))
	{
		if((LEDDriverTimeout--) == 0)
			return LEDDRIVER_TIMEOUT_UserCallback();
	}

	/* Transmit the slave address */
	I2C_Send7bitAddress(LEDDRIVER_I2C, driverAddr, I2C_Direction_Transmitter);

	LEDDriverTimeout = LEDDRIVER_FLAG_TIMEOUT;
	while (!I2C_CheckEvent(LEDDRIVER_I2C, I2C_EVENT_MASTER_TRANSMITTER_MODE_SELECTED))
	{
		if((LEDDriverTimeout--) == 0)
			return LEDDRIVER_TIMEOUT_UserCallback();
	}

	/* Transmit the address for write operation */
	I2C_SendData(LEDDRIVER_I2C, RegisterAddr);

	/* Test on EV8 and clear it */
	LEDDriverTimeout = LEDDRIVER_FLAG_TIMEOUT;
	while (!I2C_CheckEvent(LEDDRIVER_I2C, I2C_EVENT_MASTER_BYTE_TRANSMITTING))
	{
		if((LEDDriverTimeout--) == 0)
			return LEDDRIVER_TIMEOUT_UserCallback();
	}

	/* Prepare the register value to be sent */
	I2C_SendData(LEDDRIVER_I2C, RegisterValue);

	/*!< Wait till all data have been physically transferred on the bus */
	LEDDriverTimeout = LEDDRIVER_LONG_TIMEOUT;
	while(!I2C_GetFlagStatus(LEDDRIVER_I2C, I2C_FLAG_BTF))
	{
		if((LEDDriverTimeout--) == 0)
			return LEDDRIVER_TIMEOUT_UserCallback();
	}

	/* End the configuration sequence */
	I2C_GenerateSTOP(LEDDRIVER_I2C, ENABLE);

	/* Return the verifying value: 0 (Passed) or 1 (Failed) */
	return result;
}


inline uint32_t LEDDriver_startxfer(uint8_t driverAddr){
	uint32_t result=0;

	driverAddr = PCA9685_I2C_BASE_ADDRESS | (driverAddr << 1);

	/*!< While the bus is busy */
	LEDDriverTimeout = LEDDRIVER_LONG_TIMEOUT;
	while(I2C_GetFlagStatus(LEDDRIVER_I2C, I2C_FLAG_BUSY))
	{
		if((LEDDriverTimeout--) == 0)
			return LEDDRIVER_TIMEOUT_UserCallback();
	}

	/* Start the config sequence */
	I2C_GenerateSTART(LEDDRIVER_I2C, ENABLE);

	LEDDriverTimeout = LEDDRIVER_FLAG_TIMEOUT;
	while (!I2C_CheckEvent(LEDDRIVER_I2C, I2C_EVENT_MASTER_MODE_SELECT))
	{
		if((LEDDriverTimeout--) == 0)
			return LEDDRIVER_TIMEOUT_UserCallback();
	}

	/* Transmit the slave address */
	I2C_Send7bitAddress(LEDDRIVER_I2C, driverAddr, I2C_Direction_Transmitter);

	LEDDriverTimeout = LEDDRIVER_FLAG_TIMEOUT;
	while (!I2C_CheckEvent(LEDDRIVER_I2C, I2C_EVENT_MASTER_TRANSMITTER_MODE_SELECTED))
	{
		if((LEDDriverTimeout--) == 0)
			return LEDDRIVER_TIMEOUT_UserCallback();
	}

	return result;
}

inline uint32_t LEDDriver_senddata(uint8_t data){
	uint32_t result=0;

	/* Transmit the data for write operation */
	I2C_SendData(LEDDRIVER_I2C, data);

	LEDDriverTimeout = LEDDRIVER_FLAG_TIMEOUT;
	while (!I2C_CheckEvent(LEDDRIVER_I2C, I2C_EVENT_MASTER_BYTE_TRANSMITTING))
	{
		if((LEDDriverTimeout--) == 0)
			return LEDDRIVER_TIMEOUT_UserCallback();
	}
	return result;

}

inline void LEDDriver_endxfer(void){
	/* End the configuration sequence */
	I2C_GenerateSTOP(LEDDRIVER_I2C, ENABLE);

}


void LEDDriver_set_LED_ring(uint16_t ring[20][3], uint16_t env_out[6][3]){
	uint8_t i,driverAddr;

	for (driverAddr=0;driverAddr<4;driverAddr++){
		LEDDriver_startxfer(driverAddr);
		LEDDriver_senddata(PCA9685_LED0);

		for (i=driverAddr*5;i<(5+(driverAddr*5));i++){
			LEDDriver_senddata(0); //on-time = 0
			LEDDriver_senddata(0);
			LEDDriver_senddata(ring[i][0] & 0xFF); //off-time = brightness
			LEDDriver_senddata((ring[i][0] >> 8) & 0xFF);

			LEDDriver_senddata(0); //on-time = 0
			LEDDriver_senddata(0);
			LEDDriver_senddata(ring[i][1] & 0xFF); //off-time = brightness
			LEDDriver_senddata((ring[i][1] >> 8) & 0xFF);

			LEDDriver_senddata(0); //on-time = 0
			LEDDriver_senddata(0);
			LEDDriver_senddata(ring[i][2] & 0xFF); //off-time = brightness
			LEDDriver_senddata((ring[i][2] >> 8) & 0xFF);

		}

		//Channel 6 ENVOUT LED: Blue
		if (driverAddr==2){
			LEDDriver_senddata(0);
			LEDDriver_senddata(0);
			LEDDriver_senddata(env_out[5][2] & 0xFF);
			LEDDriver_senddata((env_out[5][2] >> 8) & 0xFF);
		}

		//Channel 6 ENVOUT LED: Green
		if (driverAddr==3){
			LEDDriver_senddata(0);
			LEDDriver_senddata(0);
			LEDDriver_senddata(env_out[5][1] & 0xFF);
			LEDDriver_senddata((env_out[5][1] >> 8) & 0xFF);
		}

		LEDDriver_endxfer();
	}

	driverAddr=4;
	LEDDriver_startxfer(driverAddr);
	LEDDriver_senddata(PCA9685_LED0);

	for (i=0;i<5;i++){
		LEDDriver_senddata(0); //on-time = 0
		LEDDriver_senddata(0);
		LEDDriver_senddata(env_out[i][0] & 0xFF); //off-time = brightness
		LEDDriver_senddata((env_out[i][0] >> 8) & 0xFF);

		LEDDriver_senddata(0); //on-time = 0
		LEDDriver_senddata(0);
		LEDDriver_senddata(env_out[i][1] & 0xFF); //off-time = brightness
		LEDDriver_senddata((env_out[i][1] >> 8) & 0xFF);

		LEDDriver_senddata(0); //on-time = 0
		LEDDriver_senddata(0);
		LEDDriver_senddata(env_out[i][2] & 0xFF); //off-time = brightness
		LEDDriver_senddata((env_out[i][2] >> 8) & 0xFF);

	}

	//Channel 6 ENVOUT LED: Red
	LEDDriver_senddata(0); //on-time = 0
	LEDDriver_senddata(0);
	LEDDriver_senddata(env_out[5][0] & 0xFF); //off-time = brightness
	LEDDriver_senddata((env_out[5][0] >> 8) & 0xFF);

	LEDDriver_endxfer();

}

void LEDDriver_setallLEDs(uint8_t driverAddr, uint32_t rgb1, uint32_t rgb2, uint32_t rgb3, uint32_t rgb4, uint32_t rgb5){
		uint8_t led_number;

		LEDDriver_startxfer(driverAddr);
		LEDDriver_senddata(PCA9685_LED0);

		LEDDriver_senddata(0); //on-time = 0
		LEDDriver_senddata(0);
		LEDDriver_senddata((rgb1 >> 20) & 0xFF); //off-time = brightness
		LEDDriver_senddata((rgb1 >> 28) & 0xFF);

		LEDDriver_senddata(0); //on-time = 0
		LEDDriver_senddata(0);
		LEDDriver_senddata((rgb1 >> 10) & 0xFF); //off-time = brightness
		LEDDriver_senddata((rgb1 >> 18) & 0xFF);

		LEDDriver_senddata(0); //on-time = 0
		LEDDriver_senddata(0);
		LEDDriver_senddata(rgb1 & 0xFF); //off-time = brightness
		LEDDriver_senddata((rgb1 >> 8) & 0xFF);

		LEDDriver_senddata(0); //on-time = 0
		LEDDriver_senddata(0);
		LEDDriver_senddata((rgb2 >> 20) & 0xFF); //off-time = brightness
		LEDDriver_senddata((rgb2 >> 28) & 0xFF);

		LEDDriver_senddata(0); //on-time = 0
		LEDDriver_senddata(0);
		LEDDriver_senddata((rgb2 >> 10) & 0xFF); //off-time = brightness
		LEDDriver_senddata((rgb2 >> 18) & 0xFF);

		LEDDriver_senddata(0); //on-time = 0
		LEDDriver_senddata(0);
		LEDDriver_senddata(rgb2 & 0xFF); //off-time = brightness
		LEDDriver_senddata((rgb2 >> 8) & 0xFF);

		LEDDriver_senddata(0); //on-time = 0
		LEDDriver_senddata(0);
		LEDDriver_senddata((rgb3 >> 20) & 0xFF); //off-time = brightness
		LEDDriver_senddata((rgb3 >> 28) & 0xFF);

		LEDDriver_senddata(0); //on-time = 0
		LEDDriver_senddata(0);
		LEDDriver_senddata((rgb3 >> 10) & 0xFF); //off-time = brightness
		LEDDriver_senddata((rgb3 >> 18) & 0xFF);

		LEDDriver_senddata(0); //on-time = 0
		LEDDriver_senddata(0);
		LEDDriver_senddata(rgb3 & 0xFF); //off-time = brightness
		LEDDriver_senddata((rgb3 >> 8) & 0xFF);

		LEDDriver_senddata(0); //on-time = 0
		LEDDriver_senddata(0);
		LEDDriver_senddata((rgb4 >> 20) & 0xFF); //off-time = brightness
		LEDDriver_senddata((rgb4 >> 28) & 0xFF);

		LEDDriver_senddata(0); //on-time = 0
		LEDDriver_senddata(0);
		LEDDriver_senddata((rgb4 >> 10) & 0xFF); //off-time = brightness
		LEDDriver_senddata((rgb4 >> 18) & 0xFF);

		LEDDriver_senddata(0); //on-time = 0
		LEDDriver_senddata(0);
		LEDDriver_senddata(rgb4 & 0xFF); //off-time = brightness
		LEDDriver_senddata((rgb4 >> 8) & 0xFF);

		LEDDriver_senddata(0); //on-time = 0
		LEDDriver_senddata(0);
		LEDDriver_senddata((rgb5 >> 20) & 0xFF); //off-time = brightness
		LEDDriver_senddata((rgb5 >> 28) & 0xFF);

		LEDDriver_senddata(0); //on-time = 0
		LEDDriver_senddata(0);
		LEDDriver_senddata((rgb5 >> 10) & 0xFF); //off-time = brightness
		LEDDriver_senddata((rgb5 >> 18) & 0xFF);

		LEDDriver_senddata(0); //on-time = 0
		LEDDriver_senddata(0);
		LEDDriver_senddata(rgb5 & 0xFF); //off-time = brightness
		LEDDriver_senddata((rgb5 >> 8) & 0xFF);


		LEDDriver_endxfer();
}

void LEDDriver_setRGBLED(uint8_t led_number, uint32_t rgb){ //10+10+10 bit color

	if (led_number >=0 && led_number <= 19)	{

		uint8_t driverAddr;

		driverAddr = (led_number/5);
		led_number = led_number - (driverAddr * 5);

		uint16_t c_red= (rgb >> 20) & 0b1111111111;
		uint16_t c_green= (rgb >> 10) & 0b1111111111;
		uint16_t c_blue= rgb & 0b1111111111;

		LEDDriver_startxfer(driverAddr); //20us

		LEDDriver_senddata(PCA9685_LED0 + (led_number*12));
		LEDDriver_senddata(0); //on-time = 0
		LEDDriver_senddata(0);
		LEDDriver_senddata(c_red & 0xFF); //off-time = brightness
		LEDDriver_senddata(c_red >> 8);

		LEDDriver_senddata(0); //on-time = 0 //four senddata commands take 140us
		LEDDriver_senddata(0);
		LEDDriver_senddata(c_green & 0xFF); //off-time = brightness
		LEDDriver_senddata(c_green >> 8);

		LEDDriver_senddata(0); //on-time = 0
		LEDDriver_senddata(0);
		LEDDriver_senddata(c_blue & 0xFF); //off-time = brightness
		LEDDriver_senddata(c_blue >> 8);

		LEDDriver_endxfer();
	}

}

void LEDDriver_Reset(uint8_t driverAddr){


	LEDDriver_writeregister(driverAddr, PCA9685_MODE1, 0b00000000); // clear sleep mode

	delay(20);

	//LEDDriver_writeregister(driverAddr, PCA9685_MODE1, 0b10100000);	// set up for auto increment

	LEDDriver_writeregister(driverAddr, PCA9685_MODE1, 0b10000000); //start reset mode

	delay(20);

	LEDDriver_writeregister(driverAddr, PCA9685_MODE1, 0b00100000);	//auto increment

	LEDDriver_writeregister(driverAddr, PCA9685_MODE2, 0b00010001);	// INVERT=1, OUTDRV=0, OUTNE=01



}

void LEDDriver_Init(uint8_t numdrivers){

	uint8_t i;

	LEDDriver_GPIO_Init();
	LEDDriver_I2C_Init();

	for (i=0;i<numdrivers;i++){
		LEDDriver_Reset(i);
	}

}






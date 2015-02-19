/*
 * led_driver.h
 *
 *  Created on: Dec 10, 2014
 *      Author: design
 */

#ifndef LED_DRIVER_H_
#define LED_DRIVER_H_

/* I2C peripheral configuration defines (control interface of the audio codec) */
#define LEDDRIVER_I2C                      I2C1
#define LEDDRIVER_I2C_CLK                  RCC_APB1Periph_I2C1
#define LEDDRIVER_I2C_GPIO_CLOCK           RCC_AHB1Periph_GPIOB
#define LEDDRIVER_I2C_GPIO_AF              GPIO_AF_I2C1
#define LEDDRIVER_I2C_GPIO                 GPIOB
#define LEDDRIVER_I2C_SCL_PIN              GPIO_Pin_8
#define LEDDRIVER_I2C_SDA_PIN              GPIO_Pin_9
#define LEDDRIVER_I2S_SCL_PINSRC           GPIO_PinSource8
#define LEDDRIVER_I2S_SDA_PINSRC           GPIO_PinSource9

#define I2C1_SPEED                        250000

#define PCA9685_MODE1 0x00 // location for Mode1 register address
#define PCA9685_MODE2 0x01 // location for Mode2 reigster address
#define PCA9685_LED0 0x06 // location for start of LED0 registers
#define PRE_SCALE_MODE 0xFE //location for setting prescale (clock speed)


#define PCA9685_I2C_BASE_ADDRESS 0b10000000

#define LEDDRIVER_FLAG_TIMEOUT             ((uint32_t)0x1000)
#define LEDDRIVER_LONG_TIMEOUT             ((uint32_t)(300 * LEDDRIVER_FLAG_TIMEOUT))

void LEDDriver_set_LED_ring(uint16_t ring[20][3]);

void LEDDriver_setallLEDs(uint8_t driverAddr, uint32_t rgb1, uint32_t rgb2, uint32_t rgb3, uint32_t rgb4, uint32_t rgb5);
void LEDDriver_setLED(uint8_t driverAddr, uint8_t led_number, uint16_t ontime, uint16_t offtime);
void LEDDriver_setRGBLED(uint8_t led_number, uint32_t rgb);

void LEDDriver_Init(uint8_t numdrivers);
uint32_t LEDDriver_writeregister(uint8_t driverAddr, uint8_t RegisterAddr, uint8_t RegisterValue);

#endif /* LED_DRIVER_H_ */

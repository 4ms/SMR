/*
 * flash.c - DSP bandpass resonant filter
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

#include <stm32f4xx.h>
#include "globals.h"
#include "dig_inouts.h"


static uint32_t kSectorBaseAddress[] = {
  0x08000000,
  0x08004000,
  0x08008000,
  0x0800C000,
  0x08010000,
  0x08020000,
  0x08040000,
  0x08060000,
  0x08080000,
  0x080A0000,
  0x080C0000,
  0x080E0000
};

FLASH_Status flash_erase_sector(uint32_t address){
	uint8_t i;

	FLASH_Unlock();
	FLASH_ClearFlag(FLASH_FLAG_EOP | FLASH_FLAG_OPERR | FLASH_FLAG_WRPERR |
				  FLASH_FLAG_PGAERR | FLASH_FLAG_PGPERR|FLASH_FLAG_PGSERR);
	for (i = 0; i < 12; ++i) {
		if (address == kSectorBaseAddress[i]) {
		  FLASH_EraseSector(i * 8, VoltageRange_3);
		}
	}
	FLASH_Lock();
}

FLASH_Status flash_open_erase_sector(uint32_t address){
	uint8_t i;

	for (i = 0; i < 12; ++i) {
		if (address == kSectorBaseAddress[i]) {
		  FLASH_EraseSector(i * 8, VoltageRange_3);
		}
	}
}

FLASH_Status flash_begin_open_program(void){
	FLASH_Unlock();
	FLASH_ClearFlag(FLASH_FLAG_EOP | FLASH_FLAG_OPERR | FLASH_FLAG_WRPERR |
				  FLASH_FLAG_PGAERR | FLASH_FLAG_PGPERR|FLASH_FLAG_PGSERR);
}

FLASH_Status flash_open_program_byte(uint8_t byte, uint32_t address){
	FLASH_ProgramByte(address, byte);
}

FLASH_Status flash_open_program_word(uint32_t word, uint32_t address){
	FLASH_ProgramWord(address, word);
}

FLASH_Status flash_end_open_program(void){
	FLASH_Lock();
}


//size is in # of bytes
FLASH_Status flash_open_program_array(uint8_t* arr, uint32_t address, uint32_t size) {
	while(size--) {
		FLASH_ProgramByte(address, *arr++);
		address++;
	}
}

//size in # of bytes
FLASH_Status flash_read_array(uint8_t* arr, uint32_t address, uint32_t size) {
	while(size--) {
		*arr++ = (uint8_t)(*(__IO uint32_t*)address);
		address++;
	}
}

uint32_t flash_read_word(uint32_t address){
    return( *(__IO uint32_t*)address);
}

uint8_t flash_read_byte(uint32_t address){
    return((uint8_t) (*(__IO uint32_t*)address));
}


#include "stm32f4xx.h"
#include "globals.h"
#include "inouts.h"


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

//	FLASH_Unlock();
//	FLASH_ClearFlag(FLASH_FLAG_EOP | FLASH_FLAG_OPERR | FLASH_FLAG_WRPERR |
//				  FLASH_FLAG_PGAERR | FLASH_FLAG_PGPERR|FLASH_FLAG_PGSERR);

//	FLASH_ProgramWord(address, size);
//	address += 4;

	while(size--) {
		FLASH_ProgramByte(address, *arr++);
		address++;
	}

//	FLASH_Lock();
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


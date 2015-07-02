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

//Must be erased, does not automatically check!
FLASH_Status flash_program_word(uint32_t word, uint32_t address){
	FLASH_Unlock();
	FLASH_ClearFlag(FLASH_FLAG_EOP | FLASH_FLAG_OPERR | FLASH_FLAG_WRPERR |
				  FLASH_FLAG_PGAERR | FLASH_FLAG_PGPERR|FLASH_FLAG_PGSERR);
	FLASH_ProgramWord(address, word);
	FLASH_Lock();

}

uint32_t flash_read_word(uint32_t address){
    return( *(__IO uint32_t*)address);
}


//size in # of bytes
//programs size of array, followed by the array itself
FLASH_Status flash_program_array(uint32_t* arr, uint32_t address, uint32_t size) {
	int32_t i;

	FLASH_Unlock();
	FLASH_ClearFlag(FLASH_FLAG_EOP | FLASH_FLAG_OPERR | FLASH_FLAG_WRPERR |
				  FLASH_FLAG_PGAERR | FLASH_FLAG_PGPERR|FLASH_FLAG_PGSERR);

	FLASH_ProgramWord(address, size);
	address += 4;

	for (i = 0; i < size; i += 4) {
		FLASH_ProgramWord(address, *arr++);
		address += 4;
	}

	FLASH_Lock();
}


//size in # of bytes
//reads the size of array and the array itself
FLASH_Status flash_read_array(uint32_t* arr, uint32_t address, uint32_t* size) {
	int32_t i;

	*size=*(__IO uint32_t*)address;
	address += 4;

	for (i = 0; i < *size; i += 4) {
		*arr++ = *(__IO uint32_t*)address;
		address += 4;
	}

}

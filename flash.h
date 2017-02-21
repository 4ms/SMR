/*
 * flash.h
 *
 * Author: Dan Green (danngreen1@gmail.com)
 * 2017
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



#ifndef FLASH_H_
#define FLASH_H_

#include <stm32f4xx.h>


FLASH_Status flash_erase_sector(uint32_t address);
FLASH_Status flash_open_erase_sector(uint32_t address);
FLASH_Status flash_begin_open_program(void);
FLASH_Status flash_open_program_byte(uint8_t byte, uint32_t address);
FLASH_Status flash_open_program_word(uint32_t word, uint32_t address);
FLASH_Status flash_end_open_program(void);
FLASH_Status flash_open_program_array(uint8_t* arr, uint32_t address, uint32_t size) ;
FLASH_Status flash_read_array(uint8_t* arr, uint32_t address, uint32_t size) ;
uint32_t flash_read_word(uint32_t address);
uint8_t flash_read_byte(uint32_t address);



#endif /* FLASH_H */
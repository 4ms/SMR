# Makefile for STM32F4
# 11-10-2011 E. Brombaugh
# 2014-07-25 D Green

# Object files
OBJECTS = 	startup_stm32f4xx.o system_stm32f4xx.o hardfault.o \
			main.o codec.o i2s.o led_ring.o system_mode.o user_scales.o \
			adc.o inouts.o audio_util.o filter.o envout_pwm.o led_driver.o \
			mem.o dac.o rotary.o rotation.o params.o flash.o \
			stm32f4xx_gpio.o stm32f4xx_i2c.o stm32f4xx_rcc.o \
			stm32f4xx_spi.o stm32f4xx_dma.o stm32f4xx_adc.o misc.o \
			stm32f4xx_tim.o stm32f4xx_dac.o stm32f4xx_flash.o
		
			
 
# Linker script
LDSCRIPT = stm32f427.ld

#CFLAGS = -g2 -O3 -fno-tree-loop-distribute-patterns
CFLAGS = -g2 -Ofast -fno-tree-loop-distribute-patterns
CFLAGS += -mlittle-endian -mthumb
CFLAGS +=  -I. -DARM_MATH_CM4 -D'__FPU_PRESENT=1'
CFLAGS += -mcpu=cortex-m4 -mfloat-abi=hard

CFLAGS +=  -mfpu=fpv4-sp-d16 -fsingle-precision-constant -Wdouble-promotion

AFLAGS  = -mlittle-endian -mthumb -mcpu=cortex-m4 
LFLAGS  = -Map main.map -nostartfiles -T $(LDSCRIPT)

# Executables
ARCH = arm-none-eabi
CC = $(ARCH)-gcc
LD = $(ARCH)-ld -v
AS = $(ARCH)-as
OBJCPY = $(ARCH)-objcopy
OBJDMP = $(ARCH)-objdump
GDB = $(ARCH)-gdb

CPFLAGS = -O binary
#CPFLAGS = --output-target=binary -j .text -j .data
ODFLAGS	= -x --syms

FLASH = st-flash

# Targets
all: main.bin main.hex

clean:
	-rm -f $(OBJECTS) *.lst *.elf *.bin *.map *.dmp


stlink_flash_nobootld: main.bin
	$(FLASH) write main.bin 0x08000000
	
stlink_flash: main.bin
	$(FLASH) write main.bin 0x08008000
	
gdb_flash: main.elf
	$(GDB) -x flash_cmd.gdb -batch

disassemble: main.elf
	$(OBJDMP) -dS main.elf > main.dis
	
main.ihex: main.elf
	$(OBJCPY) --output-target=ihex main.elf main.ihex

main.hex: main.elf 
	$(OBJCPY) -O ihex $< $@

main.bin: main.elf 
	$(OBJCPY) $(CPFLAGS) main.elf main.bin
	$(OBJDMP) $(ODFLAGS) main.elf > main.dmp
	ls -l main.elf main.bin

main.elf: $(OBJECTS) $(LDSCRIPT)
	$(LD) $(LFLAGS) -o main.elf $(OBJECTS)

startup_stm32f4xx.o: startup_stm32f4xx.s
	$(AS) $(AFLAGS) startup_stm32f4xx.s -o startup_stm32f4xx.o > startup_stm32f4xx.lst

hardfault.o: hardfault.s
	$(AS) $(AFLAGS) hardfault.s -o hardfault.o > hardfault.lst

%.o: %.c %.h
	$(CC) $(CFLAGS) -c -o $@ $<
	
# Rule for building the firmware update file
wav: fsk-wav

qpsk-wav: main.bin
	python stm_audio_bootloader/qpsk/encoder.py \
		-t stm32f4 -s 48000 -b 12000 -c 6000 -p 256 \
		main.bin


fsk-wav:  main.bin
	python stm_audio_bootloader/fsk/encoder.py \
		-s 48000 -b 16 -n 8 -z 4 -p 256 -g 16384 -k 1100 \
		main.bin
	


# Makefile for STM32F4
# 11-10-2011 E. Brombaugh
# 2014-07-25 D Green

# Object files
OBJECTS = 	startup_stm32f4xx.o system_stm32f4xx.o main.o codec.o i2s.o \
			adc.o inouts.o audio_util.o filter.o envout_pwm.o led_driver.o \
			mem.o dac.o rotary.o \
			stm32f4xx_gpio.o stm32f4xx_i2c.o stm32f4xx_rcc.o \
			stm32f4xx_spi.o stm32f4xx_dma.o stm32f4xx_adc.o misc.o \
			 stm32f4xx_tim.o stm32f4xx_dac.o
		
			
 
# Linker script
LDSCRIPT = stm32f427.ld

CFLAGS = -g2 -O3 -mlittle-endian -mthumb
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

CPFLAGS = --output-target=binary -j .text -j .data
ODFLAGS	= -x --syms

FLASH = st-flash

# Targets
all: main.bin

clean:
	-rm -f $(OBJECTS) *.lst *.elf *.bin *.map *.dmp

flash: gdb_flash

stlink_flash: main.bin
	$(FLASH) write main.bin 0x08000000
	
gdb_flash: main.elf
	$(GDB) -x flash_cmd.gdb -batch

disassemble: main.elf
	$(OBJDMP) -dS main.elf > main.dis
	
dist:
	tar -c *.h *.c *.s Makefile *.cmd *.cfg openocd_doflash | gzip > minimal_hello_world.tar.gz

main.ihex: main.elf
	$(OBJCPY) --output-target=ihex main.elf main.ihex

main.bin: main.elf 
	$(OBJCPY) $(CPFLAGS) main.elf main.bin
	$(OBJDMP) $(ODFLAGS) main.elf > main.dmp
	ls -l main.elf main.bin

main.elf: $(OBJECTS) $(LDSCRIPT)
	$(LD) $(LFLAGS) -o main.elf $(OBJECTS)

startup_stm32f4xx.o: startup_stm32f4xx.s
	$(AS) $(AFLAGS) startup_stm32f4xx.s -o startup_stm32f4xx.o > startup_stm32f4xx.lst

%.o: %.c %.h
	$(CC) $(CFLAGS) -c -o $@ $<


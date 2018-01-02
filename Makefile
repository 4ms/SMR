BINARYNAME = main

STARTUP = startup_stm32f4xx
SYSTEM = system_stm32f4xx
LOADFILE = stm32f427

DEVICE = stm32/device
CORE = stm32/core
PERIPH = stm32/periph

BUILDDIR = build

SOURCES += $(wildcard $(PERIPH)/src/*.c)
SOURCES += $(DEVICE)/src/$(STARTUP).s
SOURCES += $(DEVICE)/src/$(SYSTEM).c
SOURCES += $(wildcard *.c)
SOURCES += hardfault.s

OBJECTS = $(addprefix $(BUILDDIR)/, $(addsuffix .o, $(basename $(SOURCES))))

INCLUDES += -I$(DEVICE)/include \
			-I$(CORE)/include \
			-I$(PERIPH)/include \
			-I\

ELF = $(BUILDDIR)/$(BINARYNAME).elf
HEX = $(BUILDDIR)/$(BINARYNAME).hex
BIN = $(BUILDDIR)/$(BINARYNAME).bin

ARCH = arm-none-eabi
CC = $(ARCH)-gcc
LD = $(ARCH)-ld -v
AS = $(ARCH)-as
OBJCPY = $(ARCH)-objcopy
OBJDMP = $(ARCH)-objdump
GDB = $(ARCH)-gdb

CFLAGS = -g -Ofast -fno-tree-loop-distribute-patterns 
CFLAGS += -mlittle-endian -mthumb
CFLAGS +=  -I. -DARM_MATH_CM4 -D'__FPU_PRESENT=1'  $(INCLUDES)  -DUSE_STDPERIPH_DRIVER
CFLAGS += -mcpu=cortex-m4 -mfloat-abi=hard
CFLAGS +=  -mfpu=fpv4-sp-d16 -fsingle-precision-constant -Wdouble-promotion

C0FLAGS  = -O0 -g -Wall
C0FLAGS += -mlittle-endian -mthumb 
C0FLAGS +=  -I. -DARM_MATH_CM4 -D'__FPU_PRESENT=1'  $(INCLUDES)  -DUSE_STDPERIPH_DRIVER
C0FLAGS += -mcpu=cortex-m4 -mfloat-abi=hard
C0FLAGS +=  -mfpu=fpv4-sp-d16 -fsingle-precision-constant -Wdouble-promotion 


AFLAGS  = -mlittle-endian -mthumb -mcpu=cortex-m4 

LDSCRIPT = $(DEVICE)/$(LOADFILE).ld
LFLAGS  = -Map $(BUILDDIR)/$(BINARYNAME).map -nostartfiles  -T $(LDSCRIPT)
#LFLAGS  = -nostartfiles  -T $(LDSCRIPT)  -lc -lm -lgcc

# build/system_mode.o: CFLAGS = $(C0FLAGS)


all: Makefile $(BIN) $(HEX)

$(BIN): $(ELF)
	$(OBJCPY) -O binary $< $@
	$(OBJDMP) -x --syms $< > $(addsuffix .dmp, $(basename $<))
	ls -l $@ $<

$(HEX): $(ELF)
	$(OBJCPY) --output-target=ihex $< $@

$(ELF): $(OBJECTS)
	$(LD) $(LFLAGS) -o $@ $(OBJECTS)

$(BUILDDIR)/%.o: %.c $(wildcard *.h)
	mkdir -p $(dir $@)
	$(CC) -c $(CFLAGS) $< -o $@

$(BUILDDIR)/%.o: %.s
	mkdir -p $(dir $@)
	$(AS) $(AFLAGS) $< -o $@ > $(addprefix $(BUILDDIR)/, $(addsuffix .lst, $(basename $<)))


flash: $(BIN)
	st-flash write $(BIN) 0x08008000

clean:
	rm -rf build
	
wav: fsk-wav

qpsk-wav: $(BIN)
	cd .. && python stm-audio-bootloader/qpsk/encoder.py \
		-t stm32f4 -s 48000 -b 12000 -c 6000 -p 256 \
		SMR/$(BIN)


fsk-wav: $(BIN)
	cd .. && python stm-audio-bootloader/fsk/encoder.py \
		-s 48000 -b 16 -n 8 -z 4 -p 256 -g 16384 -k 1800 \
		SMR/$(BIN)
	

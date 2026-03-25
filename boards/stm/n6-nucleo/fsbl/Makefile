#
# STM32N6 FSBL Standalone Makefile
#

CC = arm-none-eabi-gcc
OBJCOPY = arm-none-eabi-objcopy

# Compiler flags
CFLAGS = -mcpu=cortex-m7 -mthumb -mfpu=fpv5-d16 -mfloat-abi=hard
CFLAGS += -ffunction-sections -fdata-sections -fno-common -fno-builtin
CFLAGS += -Os -g -DSTM32N6
CFLAGS += -I../src

# Linker flags  
LDFLAGS = -T ../nuttx-config/scripts/fsbl.ld
LDFLAGS += -Wl,-Map=fsbl.map -nostdlib -nostartfiles
LDFLAGS += -Wl,--gc-sections

# Source files
SOURCES = fsbl_main.c

# Object files
OBJECTS = $(SOURCES:.c=.o)

# Target
TARGET = fsbl_stm32n6

.PHONY: all clean

all: $(TARGET).bin

$(TARGET).elf: $(OBJECTS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(OBJECTS)

$(TARGET).bin: $(TARGET).elf
	$(OBJCOPY) -O binary $< $@
	@echo "FSBL binary created: $(TARGET).bin ($(shell stat -c%s $(TARGET).bin) bytes)"

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJECTS) $(TARGET).elf $(TARGET).bin fsbl.map

size: $(TARGET).elf
	arm-none-eabi-size $<

install: $(TARGET).bin
	cp $(TARGET).bin ../../../build/stm_n6-nucleo_fsbl/fsbl.bin
# STM32F103C8T6 cluster ECU - bare-metal build (arm-none-eabi-gcc, no HAL/LL)
#   make            build build/cluster.elf/.bin/.hex
#   make flash      program via ST-Link + OpenOCD
#   make size       section sizes against the 63 KB / 20 KB budget
#   make misra      cppcheck MISRA C:2012 addon report
#   make clean

TARGET  := cluster
BUILD   := build

PREFIX  ?= arm-none-eabi-
CC      := $(PREFIX)gcc
OBJCOPY := $(PREFIX)objcopy
SIZE    := $(PREFIX)size

SRC_DIRS := startup mcal ecual services gfx app ../common/src
SRCS     := $(foreach d,$(SRC_DIRS),$(wildcard $(d)/*.c))
OBJS     := $(patsubst %.c,$(BUILD)/%.o,$(subst ../,,$(SRCS)))
DEPS     := $(OBJS:.o=.d)

INCLUDES := -Iconfig -Icmsis/core -Icmsis/device -Imcal -Iecual -Iservices -Igfx -Iapp \
            -I../common/inc

CPU      := -mcpu=cortex-m3 -mthumb -mfloat-abi=soft
CFLAGS   := $(CPU) -std=c11 -Os -g3 -DSTM32F103xB \
            -Wall -Wextra -Werror -Wshadow -Wundef -Wdouble-promotion -Wformat=2 \
            -Wstrict-prototypes \
            -ffunction-sections -fdata-sections -fno-common -fstack-usage \
            $(INCLUDES) -MMD -MP
LDFLAGS  := $(CPU) -T linker/stm32f103c8.ld -nostartfiles \
            --specs=nano.specs --specs=nosys.specs \
            -Wl,--gc-sections -Wl,-Map=$(BUILD)/$(TARGET).map -Wl,--print-memory-usage

.PHONY: all flash size misra clean

all: $(BUILD)/$(TARGET).elf $(BUILD)/$(TARGET).bin $(BUILD)/$(TARGET).hex

$(BUILD)/%.o: %.c
	@mkdir -p $(dir $@)
	@echo "CC  $<"
	@$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/common/src/%.o: ../common/src/%.c
	@mkdir -p $(dir $@)
	@echo "CC  $<"
	@$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/$(TARGET).elf: $(OBJS) linker/stm32f103c8.ld
	@echo "LD  $@"
	@$(CC) $(OBJS) $(LDFLAGS) -o $@
	$(SIZE) $@

$(BUILD)/$(TARGET).bin: $(BUILD)/$(TARGET).elf
	$(OBJCOPY) -O binary $< $@

$(BUILD)/$(TARGET).hex: $(BUILD)/$(TARGET).elf
	$(OBJCOPY) -O ihex $< $@

flash: $(BUILD)/$(TARGET).elf
	openocd -f interface/stlink.cfg -f target/stm32f1x.cfg \
	        -c "program $< verify reset exit"

size: $(BUILD)/$(TARGET).elf
	$(SIZE) -A $< | grep -E "isr_vector|text|data|bss|stack"

misra:
	cppcheck --addon=misra --std=c11 --enable=warning,style,performance,portability \
	         --inline-suppr --suppress=missingIncludeSystem -DSTM32F103xB -D__GNUC__=13 \
	         -Iconfig -Icmsis/core -Icmsis/device -Imcal -Iecual -Iservices -Igfx -Iapp -I../common/inc \
	         $(filter-out startup/%,$(SRCS)) 2> $(BUILD)/misra.txt || true
	@echo "MISRA report: $(BUILD)/misra.txt ($$(grep -c 'misra-c2012' $(BUILD)/misra.txt) findings)"

clean:
	rm -rf $(BUILD)

-include $(DEPS)

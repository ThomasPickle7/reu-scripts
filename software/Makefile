# Makefile for RadioHound DMA Project

# --- Toolchain ---
CROSS_COMPILE ?=
CC = $(CROSS_COMPILE)gcc
LD = $(CROSS_COMPILE)ld
OBJCOPY = $(CROSS_COMPILE)objcopy

# --- Directories ---
APP_DIR = app
BSP_DIR = bsp
DRIVERS_DIR = drivers
BUILD_DIR = build


# --- Output File Names ---
TARGET = radiohound
TARGET_ELF = $(BUILD_DIR)/$(TARGET).elf
TARGET_BIN = $(BUILD_DIR)/$(TARGET).bin


# --- Compiler and Linker Flags ---
# Use 64-bit architecture and ABI flags to match the host system.
# -march=rv64gc is standard for 64-bit RISC-V general purpose systems.
# -mabi=lp64d is the standard 64-bit ABI.
CFLAGS = -I$(INC_DIR) -O0 -g -Wall -march=rv64gc -mabi=lp64d


# --- Source Files ---
C_SRCS += $(wildcard $(APP_DIR)/*.c)
C_SRCS += $(wildcard $(BSP_DIR)/*.c)
C_SRCS += $(wildcard $(DRIVERS_DIR)/*.c)

# --- Object Files ---
OBJS = $(patsubst %.c, $(BUILD_DIR)/%.o, $(notdir $(C_SRCS)))

# --- Include Paths ---
INCLUDE_PATHS = -I$(APP_DIR) -I$(BSP_DIR) -I$(DRIVERS_DIR)


# --- Linker Flags ---
# Assumes a linker script named 'link.ld' is in the root directory.
LDFLAGS = 

# --- Executable ---
TARGET = $(BUILD_DIR)/radiohound

.PHONY: all clean

all: $(TARGET).bin

$(TARGET).bin: $(TARGET).elf
	$(OBJCOPY) -O binary $< $@

$(TARGET).elf: $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

# Rule to compile source files from the 'app' directory
$(BUILD_DIR)/%.o: $(APP_DIR)/%.c
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

# Rule to compile source files from the 'bsp' directory
$(BUILD_DIR)/%.o: $(BSP_DIR)/%.c
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

# Rule to compile source files from the 'drivers' directory
$(BUILD_DIR)/%.o: $(DRIVERS_DIR)/%.c
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf $(BUILD_DIR)
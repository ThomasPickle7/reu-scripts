# Makefile for RadioHound DMA Project

# --- Toolchain ---
CROSS_COMPILE = riscv64-unknown-elf-
CC = $(CROSS_COMPILE)gcc
LD = $(CROSS_COMPILE)ld
OBJCOPY = $(CROSS_COMPILE)objcopy

# --- Directories ---
APP_DIR = software/app
BSP_DIR = software/bsp
DRIVERS_DIR = software/drivers
BUILD_DIR = build

# --- Source Files ---
C_SRCS += $(wildcard $(APP_DIR)/*.c)
C_SRCS += $(wildcard $(BSP_DIR)/*.c)
C_SRCS += $(wildcard $(DRIVERS_DIR)/*.c)

# --- Object Files ---
OBJS = $(patsubst %.c, $(BUILD_DIR)/%.o, $(notdir $(C_SRCS)))

# --- Include Paths ---
INCLUDE_PATHS = -I$(APP_DIR) -I$(BSP_DIR) -I$(DRIVERS_DIR)

# --- Compiler Flags ---
# -mcmodel=medany is important for position-independent code.
# -O0 disables optimizations for easier debugging.
CFLAGS = -g -mcmodel=medany -Wall -Wextra -O0 $(INCLUDE_PATHS)

# --- Linker Flags ---
# Assumes a linker script named 'link.ld' is in the root directory.
LDFLAGS = -T link.ld -nostartfiles

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
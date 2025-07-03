# --- Toolchain Definition ---
# On the BeagleV, we use the native compiler, so CROSS_COMPILE is empty.
CROSS_COMPILE ?=
CC = $(CROSS_COMPILE)gcc
LD = $(CROSS_COMPILE)ld
OBJCOPY = $(CROSS_COMPILE)objcopy

# --- Project Structure ---
INC_DIR = inc
SRC_DIR = src
BUILD_DIR = build

# --- Output File Names ---
TARGET = dma_test_app
TARGET_ELF = $(BUILD_DIR)/$(TARGET).elf
TARGET_BIN = $(BUILD_DIR)/$(TARGET).bin


# --- Compiler and Linker Flags ---
# Use 64-bit architecture and ABI flags to match the host system.
# -march=rv64gc is standard for 64-bit RISC-V general purpose systems.
# -mabi=lp64d is the standard 64-bit ABI.
CFLAGS = -I$(INC_DIR) -O0 -g -Wall -march=rv64gc -mabi=lp64d

# Removed -nostdlib to allow linking with standard C libraries for printf, etc.
LDFLAGS =

# --- Source and Object Files ---
SRCS = $(wildcard $(SRC_DIR)/*.c)
OBJS = $(patsubst $(SRC_DIR)/%.c, $(BUILD_DIR)/%.o, $(SRCS))


# =============================================================================
# Makefile Rules
# =============================================================================

all: $(TARGET_ELF)

$(TARGET_ELF): $(OBJS)
	@mkdir -p $(BUILD_DIR)
	@echo "LD   $@"
	$(CC) $(LDFLAGS) $(CFLAGS) -o $@ $^

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(BUILD_DIR)
	@echo "CC   $<"
	$(CC) $(CFLAGS) -c $< -o $@

bin: $(TARGET_BIN)

$(TARGET_BIN): $(TARGET_ELF)
	$(OBJCOPY) -O binary $< $@

clean:
	@echo "CLEAN"
	@rm -rf $(BUILD_DIR)

.PHONY: all clean bin


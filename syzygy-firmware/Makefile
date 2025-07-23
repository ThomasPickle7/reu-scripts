# --- Project Structure ---
# Only expect source files directly in the src directory (no subfolders)
SRC_DIR = src
BUILD_DIR = build

# --- Output File Names ---
TARGET = apb
TARGET_ELF = $(BUILD_DIR)/$(TARGET).elf
TARGET_BIN = $(BUILD_DIR)/$(TARGET).bin

# --- Compiler and Linker Flags ---
CFLAGS = -I$(INC_DIR) -O0 -g -Wall -march=rv64gc -mabi=lp64d
LDFLAGS =

# --- Source and Object Files ---
# Only match .c files directly in src (no recursion)
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

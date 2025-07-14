#
# Makefile for the RadioHound project (Linux Userspace Version)
#

# --- Toolchain Definition ---
# Use the native gcc compiler on the BeagleV for a Linux application.
CROSS_COMPILE ?=
CC = $(CROSS_COMPILE)gcc

# --- Project Structure ---
BUILD_DIR = build
APP_DIR = app
BSP_DIR = bsp
DRIVERS_DIR = drivers

# --- Output File Names ---
TARGET = radiohound
TARGET_EXE = $(BUILD_DIR)/$(TARGET)

# --- Compiler and Linker Flags ---
# Standard flags for a 64-bit RISC-V Linux application.
CFLAGS = -O0 -g -Wall -march=rv64gc -mabi=lp64d

# Add all subdirectories to the include path.
INCLUDES = -I$(APP_DIR) -I$(BSP_DIR) -I$(DRIVERS_DIR)

# For a standard Linux app, we link against the C library, so LDFLAGS is minimal.
LDFLAGS =

# --- Source and Object Files ---
# Find all .c files in the source directories.
SRCS = $(wildcard $(APP_DIR)/*.c) $(wildcard $(BSP_DIR)/*.c) $(wildcard $(DRIVERS_DIR)/*.c)

# Create a list of object files, placing them in the build directory.
OBJS = $(patsubst %.c,$(BUILD_DIR)/%.o,$(SRCS))


# =============================================================================
# Makefile Rules
# =============================================================================

# Default rule: build the final executable.
all: $(TARGET_EXE)

# Rule to link the final executable.
$(TARGET_EXE): $(OBJS)
	@mkdir -p $(BUILD_DIR)
	@echo "LD   $@"
	$(CC) $(LDFLAGS) $^ -o $@

# Pattern rule to compile any .c file into its corresponding .o file.
# This single rule handles all source directories.
$(BUILD_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	@echo "CC   $<"
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

# Rule to clean up all build artifacts.
clean:
	@echo "CLEAN"
	@rm -rf $(BUILD_DIR)

# Phony targets are not real files.
.PHONY: all clean

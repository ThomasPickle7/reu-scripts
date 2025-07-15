# Application Name
TARGET := radiohound

# Build directory
BUILD_DIR := build

# Compiler and flags
CC := gcc
CFLAGS := -O0 -g -Wall -march=rv64gc -mabi=lp64d
LDFLAGS :=

# Source directories
APP_DIR := app
BSP_DIR := bsp
DRV_DIR := drivers

# Include directories
INCLUDES := -I$(APP_DIR) -I$(BSP_DIR) -I$(DRV_DIR)

# ------------------------------------------------------------------------------
# Simplified Source List: We only need to compile the consolidated main.c
# ------------------------------------------------------------------------------
C_SRCS := $(APP_DIR)/main.c

# Object files
OBJS := $(patsubst %.c, $(BUILD_DIR)/%.o, $(C_SRCS))

# Executable path
EXECUTABLE := $(BUILD_DIR)/$(TARGET)

.PHONY: all clean

all: $(EXECUTABLE)

# Rule to link the final executable
$(EXECUTABLE): $(OBJS)
	@echo "LD   $(EXECUTABLE)"
	@$(CC) $(OBJS) -o $@ $(LDFLAGS)

# Rule to compile C source files into object files
$(BUILD_DIR)/%.o: %.c
	@echo "CC   $<"
	@mkdir -p $(dir $@)
	@$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

# Clean the build directory
clean:
	@echo "CLEAN"
	@rm -rf $(BUILD_DIR)
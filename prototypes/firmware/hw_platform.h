
// =================================================================================================
// File: hw_platform.h
// Description: Defines hardware-specific information like device names and register maps.
// =================================================================================================

#ifndef HW_PLATFORM_H
#define HW_PLATFORM_H

#include <stdint.h>

// --- Linux Device File Names ---
#define UIO_DMA_DEV_NAME        "/dev/uio0"
#define UIO_STREAM_SRC_DEV_NAME "/dev/uio1"
#define UDMABUF_DEVICE_NAME     "/dev/udmabuf-ddr-nc0"

// --- Register Map for the Custom AXI Stream Source IP ---
// The 'volatile' keyword is crucial. It tells the compiler that the value in memory
// can be changed by external hardware at any time, preventing optimizations that
// might lead to reading stale data from a CPU cache/register.
typedef volatile struct {
    uint32_t CONTROL_REG;     // Offset 0x00: Write a 1 to start (pulsed)
    uint32_t STATUS_REG;      // Offset 0x04: Read status (e.g., busy, done)
    uint32_t RESERVED1[2];    // Offset 0x08, 0x0C
    uint32_t NUM_BYTES_REG;   // Offset 0x10: Number of bytes to generate
    uint32_t DEST_REG;        // Offset 0x14: TDEST value for the AXI Stream
} AxiStreamSource_Regs_t;


// --- Register Map for the Custom DMA Controller IP ---
// Note: This is a simplified version based on the original code's usage.
// A real driver would define all registers.
#define FDMA_MAX_STREAMS 4
#define FDMA_MAX_MEMORY_WINDOWS 4

typedef volatile struct {
    uint32_t ID_REG;
    uint32_t CONFIG_REG;
    uint32_t START_OPERATION_REG;
    uint32_t MPU_PROTECT_REG[FDMA_MAX_MEMORY_WINDOWS];
    uint32_t INTR_0_STAT_REG;
    uint32_t INTR_0_MASK_REG;
    uint32_t INTR_1_STAT_REG;
    uint32_t INTR_1_MASK_REG;
    uint32_t STREAM_ADDR_REG[FDMA_MAX_STREAMS];
    // ... other registers from original code ...
} Dma_Regs_t;


// --- DMA Stream Descriptor Structure ---
// This structure is NOT a register map. It defines the format of the control
// structure that the CPU writes into the shared DMA memory buffer. The DMA
// controller reads this structure to get its instructions.
typedef volatile struct {
    uint32_t SRC_ADDR_REG;      // Source Address (not used for stream-to-memory)
    uint32_t DEST_ADDR_REG;     // Physical destination address in DDR
    uint32_t BYTE_COUNT_REG;    // Number of bytes to transfer
    uint32_t CONFIG_REG;        // Configuration flags for the transfer
} DmaStreamDescriptor_t;


// --- Bit Definitions for DMA Registers and Descriptors ---

// For START_OPERATION_REG
#define FDMA_START_STREAM(id)         (1 << (id + 8))

// For INTR_0_MASK_REG
#define FDMA_IRQ_MASK_STREAM_DONE(id) (1 << (id + 8))

// For DmaStreamDescriptor_t.CONFIG_REG
#define STREAM_OP_INCR                (1 << 0) // Incrementing destination address
#define STREAM_FLAG_IRQ_EN            (1 << 1) // Enable interrupt on completion
#define STREAM_FLAG_DEST_RDY          (1 << 2) // Flow control: Destination is ready
#define STREAM_FLAG_VALID             (1 << 3) // This descriptor is valid for processing

#endif // HW_PLATFORM_H




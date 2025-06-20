#ifndef DMA_DRIVER_H
#define DMA_DRIVER_H

#include <stdint.h>
#include <stddef.h>

// Internal Buffer Descriptor structure (for mem-to-mem)
typedef struct {
    volatile uint32_t CONFIG_REG;
    volatile uint32_t BYTE_COUNT_REG;
    volatile uint32_t SOURCE_ADDR_REG;
    volatile uint32_t DEST_ADDR_REG;
    volatile uint32_t NEXT_DESC_ADDR_REG;
} DmaDescriptor_t;

// Full register map of the DMA Controller
typedef struct {
    volatile const uint32_t VERSION_REG;            // 0x000
    volatile uint32_t       START_OPERATION_REG;    // 0x004
    uint8_t                 _RESERVED1[0x10 - 0x08];
    volatile const uint32_t INTR_0_STAT_REG;        // 0x010
    volatile uint32_t       INTR_0_MASK_REG;        // 0x014
    volatile uint32_t       INTR_0_CLEAR_REG;       // 0x018
    uint8_t                 _RESERVED2[0x60 - 0x20];
    DmaDescriptor_t         DESCRIPTOR[4];          // Internal Descriptors start at 0x060


} CoreAXI4DMAController_Regs_t;

// Bits for the Internal Buffer Descriptor's CONFIG_REG
#define DESC_CONFIG_SOURCE_OP_INCR      (0b01 << 0)
#define DESC_CONFIG_DEST_OP_INCR        (0b01 << 2)
#define DESC_CONFIG_CHAIN               (1U << 10)
#define DESC_CONFIG_INTR_ON_PROCESS     (1U << 12)
#define DESC_CONFIG_SOURCE_DATA_VALID   (1U << 13)
#define DESC_CONFIG_DEST_DATA_READY     (1U << 14)
#define DESC_CONFIG_DESCRIPTOR_VALID    (1U << 15)

/****************************************************************
 * PUBLIC DRIVER API
 ****************************************************************/

int DMA_MapRegisters(void);
void DMA_UnmapRegisters(void);

/**
 * @brief Runs a memory-to-memory loopback test on the DMA.
 * This function initializes a source buffer with a data pattern, configures a
 * descriptor to copy it to a destination buffer, starts the DMA, and waits for
 * completion. It then verifies the data.
 * @return 1 on PASS, 0 on FAIL.
 */
int DMA_RunMemoryLoopbackTest(void);

// Interrupt handling
int DMA_GetInterruptStatus(void);
void DMA_ClearInterrupt(void);

#endif // DMA_DRIVER_H

#ifndef DMA_DRIVER_H
#define DMA_DRIVER_H

#include <stdint.h>
#include <stddef.h>

// The descriptor that lives in system memory for stream transfers
typedef struct {
    volatile uint32_t CONFIG_REG;
    volatile uint32_t BYTE_COUNT_REG;
    volatile uint32_t DEST_ADDR_REG;
} StreamDescriptor_t;

// The internal descriptors (for mem-to-mem only)
typedef struct {
    volatile uint32_t CONFIG_REG;
    volatile uint32_t BYTE_COUNT_REG;
    volatile uint32_t SOURCE_ADDR_REG;
    volatile uint32_t DEST_ADDR_REG;
    volatile uint32_t NEXT_DESC_ADDR_REG;
} DmaDescriptor_t;

// The full register map of the DMA Controller IP
typedef struct {
    volatile const uint32_t VERSION_REG;            // 0x000
    volatile uint32_t       START_OPERATION_REG;    // 0x004
    uint8_t                 _RESERVED1[0x10 - 0x08];
    volatile const uint32_t INTR_0_STAT_REG;        // 0x010
    volatile uint32_t       INTR_0_MASK_REG;        // 0x014
    volatile uint32_t       INTR_0_CLEAR_REG;       // 0x018
    volatile const uint32_t INTR_0_EXT_ADDR_REG;    // 0x01C
    uint8_t                 _RESERVED2[0x60 - 0x20];
    DmaDescriptor_t         DESCRIPTOR[4];          // 0x060 - 0x0B0
    uint8_t                 _RESERVED3[0x460 - 0x0B0]; // Padding
    volatile uint32_t       STREAM_0_ADDR_REG;      // 0x460
    volatile uint32_t       STREAM_1_ADDR_REG;      // 0x464
    volatile uint32_t       STREAM_2_ADDR_REG;      // 0x468
    volatile uint32_t       STREAM_3_ADDR_REG;      // 0x46C
} CoreAXI4DMAController_Regs_t;

// Bits for the Stream Descriptor CONFIG_REG
#define STREAM_DESC_CONFIG_DEST_OP_INCR   (0b01 << 0)
#define STREAM_DESC_CONFIG_DATA_READY     (1U << 2)
#define STREAM_DESC_CONFIG_VALID          (1U << 3)


/****************************************************************
 * PUBLIC DRIVER API
 ****************************************************************/

int DMA_MapRegisters(void);
void DMA_UnmapRegisters(void);

// New API functions for the correct handshake
int DMA_ArmStream(uintptr_t descriptor_phys_addr, uintptr_t buffer_phys_addr, size_t buffer_size);
void DMA_ProvideBuffer(uintptr_t descriptor_phys_addr);

int DMA_GetInterruptStatus(void);
void DMA_ClearInterrupt(void);

// Helper to map and print the contents of the physical data buffer
void DMA_PrintDataBuffer(uintptr_t buffer_phys_addr, size_t bytes_to_print);

#endif // DMA_DRIVER_H

#ifndef DMA_DRIVER_H
#define DMA_DRIVER_H

#include <stdint.h>
#include <stddef.h>

/**
 * @brief Defines the structure for a Stream Descriptor in memory.
 * This must match the format in the CoreAXI4DMAController datasheet (Figure 1-3).
 */
typedef struct {
    volatile uint32_t CONFIG_REG;
    volatile uint32_t BYTE_COUNT_REG;
    volatile uint32_t DEST_ADDR_REG;
} StreamDescriptor_t;

/**
 * @brief Defines the structure for the Internal Buffer Descriptors.
 * This is used for memory-to-memory transfers, not stream-to-memory.
 */
typedef struct {
    volatile uint32_t CONFIG_REG;
    volatile uint32_t BYTE_COUNT_REG;
    volatile uint32_t SOURCE_ADDR_REG;
    volatile uint32_t DEST_ADDR_REG;
    volatile uint32_t NEXT_DESC_ADDR_REG;
} DmaDescriptor_t;


/**
 * @brief Defines the register map for the CoreAXI4DMAController.
 * This structure now correctly includes the Stream Descriptor Address Registers at offset 0x460.
 */
typedef struct {
    volatile const uint32_t VERSION_REG;            // 0x000
    volatile uint32_t       START_OPERATION_REG;    // 0x004
    uint8_t                 _RESERVED1[0x10 - 0x08];
    volatile const uint32_t INTR_0_STAT_REG;        // 0x010
    volatile uint32_t       INTR_0_MASK_REG;        // 0x014
    volatile uint32_t       INTR_0_CLEAR_REG;       // 0x018
    volatile const uint32_t INTR_0_EXT_ADDR_REG;    // 0x01C
    uint8_t                 _RESERVED2[0x60 - 0x20];
    DmaDescriptor_t         DESCRIPTOR[4];          // 0x060 to 0x0B0 (4 descriptors * 20 bytes/desc)
    uint8_t                 _RESERVED3[0x460 - 0x0B0]; // Padding to get to the stream registers
    volatile uint32_t       STREAM_0_ADDR_REG;      // 0x460
    volatile uint32_t       STREAM_1_ADDR_REG;      // 0x464
    volatile uint32_t       STREAM_2_ADDR_REG;      // 0x468
    volatile uint32_t       STREAM_3_ADDR_REG;      // 0x46C
} CoreAXI4DMAController_Regs_t;


/**
 * @brief Configuration bits for the Stream Descriptor's CONFIG_REG.
 */
#define STREAM_DESC_CONFIG_DEST_OP_INCR   (0b01 << 0)
#define STREAM_DESC_CONFIG_DATA_READY     (1U << 2)
#define STREAM_DESC_CONFIG_VALID          (1U << 3)


/****************************************************************
 * PUBLIC DRIVER API
 ****************************************************************/

/**
 * @brief Maps the physical DMA controller registers into virtual memory.
 * @return 1 on success, 0 on failure.
 */
int DMA_MapRegisters(void);

/**
 * @brief Unmaps the DMA controller registers from virtual memory.
 */
void DMA_UnmapRegisters(void);

/**
 * @brief Configures the DMA for a single AXI-Stream to Memory transfer.
 * @param descriptor_phys_addr The physical memory address where the StreamDescriptor_t struct resides.
 * @param buffer_phys_addr The physical memory address of the destination buffer for the stream data.
 * @param buffer_size The size of the transfer in bytes.
 * @return 1 on success, 0 on failure.
 */
int DMA_SetupStreamToMemory(uintptr_t descriptor_phys_addr, uintptr_t buffer_phys_addr, size_t buffer_size);

/**
 * @brief Checks for a completed DMA operation.
 * @return The completed descriptor number, or -1 if none. For streams, this will be 33.
 */
int DMA_GetCompletedDescriptor(void);

/**
 * @brief Clears the operation completion interrupt flag.
 */
void DMA_ClearCompletionInterrupt(void);

#endif // DMA_DRIVER_H

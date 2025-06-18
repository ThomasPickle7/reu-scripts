#ifndef DMA_DRIVER_H
#define DMA_DRIVER_H

#include <stdint.h>
#include <stddef.h>

typedef struct {
    volatile uint32_t CONFIG_REG;
    volatile uint32_t BYTE_COUNT_REG;
    volatile uint32_t SOURCE_ADDR_REG;
    volatile uint32_t DEST_ADDR_REG;
    volatile uint32_t NEXT_DESC_ADDR_REG;
} DmaDescriptor_t;

typedef struct {
    volatile const uint32_t VERSION_REG;
    volatile uint32_t       START_OPERATION_REG;
    uint8_t                 _RESERVED1[0x10 - 0x08];
    volatile const uint32_t INTR_0_STAT_REG;
    volatile uint32_t       INTR_0_MASK_REG;
    volatile uint32_t       INTR_0_CLEAR_REG;
    volatile const uint32_t INTR_0_EXT_ADDR_REG;
    uint8_t                 _RESERVED2[0x60 - 0x20];
    DmaDescriptor_t         DESCRIPTOR[32];
} CoreAXI4DMAController_Regs_t;

#define DESC_CONFIG_CHAIN               (1U << 10)
#define DESC_CONFIG_INTR_ON_PROCESS     (1U << 12)
#define DESC_CONFIG_SOURCE_DATA_VALID   (1U << 13)
#define DESC_CONFIG_DEST_DATA_READY     (1U << 14)
#define DESC_CONFIG_DESCRIPTOR_VALID    (1U << 15)

#define DESC_OPR_NO_OP                  (0b00)
#define DESC_OPR_INCREMENTING           (0b01)

/****************************************************************
 * PUBLIC DRIVER API
 ****************************************************************/

int DMA_MapRegisters(void);
void DMA_UnmapRegisters(void);
int DMA_InitCyclicStream(uint8_t num_descriptors, volatile void* buffers[], size_t buffer_size);

/**
 * @brief Verifies the configuration of a descriptor by reading back written values.
 * This function is for debugging to confirm the CPU-to-DMA control path is working.
 * @param descriptor_num The descriptor to verify.
 * @param buffers The array of buffer pointers (used to verify the destination address).
 * @param buffer_size The size of the buffer (used to verify the byte count).
 * @return 1 if verification passes, 0 on failure.
 */
int DMA_VerifyConfig(uint8_t descriptor_num, volatile void* buffers[], size_t buffer_size);

void DMA_StartCyclic(uint8_t start_descriptor_num);
int DMA_GetCompletedDescriptor(void);
void DMA_ClearCompletionInterrupt(void);
void DMA_ReturnBuffer(uint8_t descriptor_num);
void DMA_DebugDumpRegisters(uint8_t descriptor_num);

#endif

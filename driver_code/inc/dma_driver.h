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
    volatile const uint32_t VERSION_REG;            // DMA Controller Version Register
    volatile uint32_t       START_OPERATION_REG;    // DMA Start Operation Register
    uint8_t                 _RESERVED1[0x10 - 0x08]; // Reserved space for alignment
    volatile const uint32_t INTR_0_STAT_REG;        // Interrupt Status Register for Descriptor 0
    volatile uint32_t       INTR_0_MASK_REG;        // Interrupt Mask Register for Descriptor 0
    volatile uint32_t       INTR_0_CLEAR_REG;       // Interrupt Clear Register for Descriptor 0    
    uint8_t                 _RESERVED2[0x60 - 0x20]; // Reserved space for alignment
    DmaDescriptor_t         DESCRIPTOR[4];          // Internal Buffer Descriptors (0-3)


} CoreAXI4DMAController_Regs_t;

// Bits for the Internal Buffer Descriptor's CONFIG_REG
#define DESC_CONFIG_SOURCE_OP_INCR      (0b01 << 0) // Source address increment
#define DESC_CONFIG_DEST_OP_INCR        (0b01 << 2) // Destination address increment
#define DESC_CONFIG_CHAIN               (1U << 10) // Enable chaining to next descriptor
#define DESC_CONFIG_INTR_ON_PROCESS     (1U << 12) // Interrupt on process completion
#define DESC_CONFIG_SOURCE_DATA_VALID   (1U << 13) // Source data is valid
#define DESC_CONFIG_DEST_DATA_READY     (1U << 14) // Destination data is ready
#define DESC_CONFIG_DESCRIPTOR_VALID    (1U << 15) // Descriptor is valid for processing


/**
 * @brief Maps the DMA controller registers into the process's address space.
 * This function opens /dev/mem, maps the DMA controller's registers, and
 * returns a pointer to the mapped registers.
 */
int DMA_MapRegisters(void);

/**
 * @brief Unmaps the DMA controller registers from the process's address space.
 * This function cleans up the memory mapping created by DMA_MapRegisters.
 * It should be called before the program exits to avoid memory leaks.
 */
void DMA_UnmapRegisters(void);

/**
 * @brief Runs a memory-to-memory loopback test on the DMA.
 * This function initializes a source buffer with a data pattern, configures a
 * descriptor to copy it to a destination buffer, starts the DMA, and waits for
 * completion. It then verifies the data.
 * @return 1 on PASS, 0 on FAIL.
 */
int DMA_RunMemoryLoopbackTest(void);

/**
 * @brief Gets the current interrupt status for Descriptor 0.
 * This function checks the interrupt status register for Descriptor 0 and
 * returns the status bits.
 */
int DMA_GetInterruptStatus(void);

/**
 * @brief Clears the interrupt status for Descriptor 0.
 * This function clears the interrupt status register for Descriptor 0.
 */
void DMA_ClearInterrupt(void);

#endif // DMA_DRIVER_H

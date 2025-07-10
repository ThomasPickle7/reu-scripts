/**************************************************************************************************
 * @file dma_driver.h
 * @author Thomas Pickle
 * @brief AXI DMA Driver for CoreAXI4DMAController
 * @version 0.2
 * @date 2024-07-10
 *
 * @copyright Copyright (c) 2024
 *
 *************************************************************************************************/
#ifndef DMA_DRIVER_H_
#define DMA_DRIVER_H_

#include <stdint.h>
#include "hw_platform.h"

// --- Bitfield Flags for Memory-to-Memory Descriptors ---
#define MEM_OP_INCR             (0b01)      // Increment address after each transfer
#define MEM_FLAG_CHAIN          (1U << 10)  // This descriptor points to another
#define MEM_FLAG_IRQ_ON_PROCESS (1U << 12)  // Trigger interrupt when this descriptor is processed
#define MEM_FLAG_SRC_RDY        (1U << 13)  // Source buffer is ready
#define MEM_FLAG_DEST_RDY       (1U << 14)  // Destination buffer is ready
#define MEM_FLAG_VALID          (1U << 15)  // This descriptor is valid for processing

// --- Bitfield Flags for Start Operation Register ---
#define FDMA_START_MEM(n)       (1U << (n)) // Start memory channel 'n'

// --- Bitfield Flags for Interrupt Registers ---
#define FDMA_IRQ_MASK_ALL       (0x0FU)     // Mask to enable all interrupt types
#define FDMA_IRQ_CLEAR_ALL      (0x0FU)     // Mask to clear all pending interrupts
#define FDMA_IRQ_STAT_WR_ERR    (1U << 1)   // Write error occurred
#define FDMA_IRQ_STAT_INVALID_DESC (1U << 3) // Invalid descriptor was read

/**************************************************************************************************
 * @brief Structure for a single DMA descriptor block in memory.
 * @note This must be located in a memory region accessible by the DMA controller.
 *************************************************************************************************/
typedef struct {
    volatile uint32_t CONFIG_REG;
    volatile uint32_t BYTE_COUNT_REG;
    volatile uint32_t SOURCE_ADDR_REG;
    volatile uint32_t DEST_ADDR_REG;
    volatile uint32_t NEXT_DESC_ADDR_REG;
    uint8_t           _RESERVED[0x20 - 0x14];
} DmaDescriptorBlock_t;

/**************************************************************************************************
 * @brief Structure for the CoreAXI4DMAController memory-mapped registers.
 *************************************************************************************************/
typedef struct {
    volatile const uint32_t VERSION_REG;
    volatile uint32_t       START_OPERATION_REG;
    uint8_t                 _RESERVED1[0x10 - 0x08];
    volatile const uint32_t INTR_0_STAT_REG;
    volatile uint32_t       INTR_0_MASK_REG;
    volatile uint32_t       INTR_0_CLEAR_REG;
    uint8_t                 _RESERVED2[0x60 - 0x1C];
    DmaDescriptorBlock_t    DESCRIPTOR[32]; // Internal descriptors
    volatile uint32_t       STREAM_ADDR_REG[4];
} CoreAXI4DMAController_Regs_t;

/**************************************************************************************************
 * @brief A handle for a DMA instance, pointing to its registers.
 *************************************************************************************************/
typedef struct {
    CoreAXI4DMAController_Regs_t *regs;
    uint32_t irq_num;
} dma_instance_t;

/**************************************************************************************************
 * @brief Initializes the DMA controller and its interrupt system.
 *
 * @param dma The DMA instance to initialize.
 * @param base_address The physical base address of the DMA controller's registers.
 *************************************************************************************************/
void dma_init(dma_instance_t *dma, uint32_t base_address);

/**************************************************************************************************
 * @brief Forcibly stops all DMA activity by clearing all descriptor valid bits.
 *
 * @param dma The DMA instance to stop.
 *************************************************************************************************/
void dma_force_stop(dma_instance_t *dma);

/**************************************************************************************************
 * @brief Resets the interrupt system for the DMA controller.
 * @note This clears any pending interrupts and sets the mask to 0.
 *
 * @param dma The DMA instance to reset.
 *************************************************************************************************/
void dma_reset_interrupts(dma_instance_t *dma);

/**************************************************************************************************
 * @brief Configures a single internal memory-to-memory descriptor.
 *
 * @param dma The DMA instance.
 * @param index The descriptor index (0-31).
 * @param src_addr Physical source address.
 * @param dest_addr Physical destination address.
 * @param byte_count Number of bytes to transfer.
 * @param next_desc_index The index of the next descriptor in the chain.
 * @param chain Whether to chain to the next descriptor.
 *************************************************************************************************/
void dma_configure_m2m_descriptor(dma_instance_t *dma, uint8_t index, uint32_t src_addr, uint32_t dest_addr, uint32_t byte_count, uint8_t next_desc_index, int chain);

/**************************************************************************************************
 * @brief Starts a memory channel transfer.
 *
 * @param dma The DMA instance.
 * @param channel_num The memory channel to start (0-15).
 *************************************************************************************************/
void dma_start_channel(dma_instance_t *dma, uint8_t channel_num);

/**************************************************************************************************
 * @brief Polls the interrupt status register until an interrupt is flagged.
 *
 * @param dma The DMA instance.
 * @return uint32_t The value of the status register.
 *************************************************************************************************/
uint32_t dma_wait_for_interrupt(dma_instance_t *dma);

/**************************************************************************************************
 * @brief Clears the specified interrupt flags.
 *
 * @param dma The DMA instance.
 * @param clear_mask A bitmask of the flags to clear (e.g., FDMA_IRQ_CLEAR_ALL).
 *************************************************************************************************/
void dma_clear_interrupt(dma_instance_t *dma, uint32_t clear_mask);

#endif /* DMA_DRIVER_H_ */

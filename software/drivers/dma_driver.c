/**************************************************************************************************
 * @file dma_driver.c
 * @author Thomas Pickle
 * @brief AXI DMA Driver for CoreAXI4DMAController
 * @version 0.2
 * @date 2024-07-10
 *
 * @copyright Copyright (c) 2024
 *
 *************************************************************************************************/

#include "dma_driver.h"

// Helper macro for memory barriers
#define memory_barrier() __sync_synchronize()

/**************************************************************************************************
 * @brief Initializes the DMA controller and its interrupt system.
 *************************************************************************************************/
void dma_init(dma_instance_t *dma, uint32_t base_address) {
    dma->regs = (CoreAXI4DMAController_Regs_t *)base_address;
    dma->irq_num = AXI_DMA_IRQ;

    // Perform a full reset on initialization
    dma_force_stop(dma);
    dma_reset_interrupts(dma);
}

/**************************************************************************************************
 * @brief Forcibly stops all DMA activity by clearing all descriptor valid bits.
 *************************************************************************************************/
void dma_force_stop(dma_instance_t *dma) {
    for (int i = 0; i < 32; ++i) {
        dma->regs->DESCRIPTOR[i].CONFIG_REG = 0;
    }
    for (int i = 0; i < 4; ++i) {
        dma->regs->STREAM_ADDR_REG[i] = 0;
    }
    memory_barrier();
}

/**************************************************************************************************
 * @brief Resets the interrupt system for the DMA controller.
 *************************************************************************************************/
void dma_reset_interrupts(dma_instance_t *dma) {
    // Disable all interrupt sources
    dma->regs->INTR_0_MASK_REG = 0;
    memory_barrier();

    // Clear any pending interrupt flags
    dma->regs->INTR_0_CLEAR_REG = FDMA_IRQ_CLEAR_ALL;
    memory_barrier();
}

/**************************************************************************************************
 * @brief Configures a single internal memory-to-memory descriptor.
 *************************************************************************************************/
void dma_configure_m2m_descriptor(dma_instance_t *dma, uint8_t index, uint32_t src_addr, uint32_t dest_addr, uint32_t byte_count, uint8_t next_desc_index, int chain) {
    if (index > 31) return;

    DmaDescriptorBlock_t *desc = &dma->regs->DESCRIPTOR[index];

    desc->SOURCE_ADDR_REG = src_addr;
    desc->DEST_ADDR_REG = dest_addr;
    desc->BYTE_COUNT_REG = byte_count;
    desc->NEXT_DESC_ADDR_REG = next_desc_index;

    uint32_t config = (MEM_OP_INCR << 2) | MEM_OP_INCR | MEM_FLAG_IRQ_ON_PROCESS | MEM_FLAG_VALID;
    if (chain) {
        config |= MEM_FLAG_CHAIN;
    }
    desc->CONFIG_REG = config;
}

/**************************************************************************************************
 * @brief Starts a memory channel transfer.
 *************************************************************************************************/
void dma_start_channel(dma_instance_t *dma, uint8_t channel_num) {
    if (channel_num > 15) return;
    dma->regs->START_OPERATION_REG = FDMA_START_MEM(channel_num);
    memory_barrier();
}

/**************************************************************************************************
 * @brief Polls the interrupt status register until an interrupt is flagged.
 *************************************************************************************************/
uint32_t dma_wait_for_interrupt(dma_instance_t *dma) {
    uint32_t status;
    while (((status = dma->regs->INTR_0_STAT_REG) & FDMA_IRQ_MASK_ALL) == 0) {
        // Busy-wait
    }
    return status;
}

/**************************************************************************************************
 * @brief Clears the specified interrupt flags.
 *************************************************************************************************/
void dma_clear_interrupt(dma_instance_t *dma, uint32_t clear_mask) {
    dma->regs->INTR_0_CLEAR_REG = clear_mask;
    memory_barrier();
}

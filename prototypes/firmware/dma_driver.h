
// =================================================================================================
// File: dma_driver.h
// Description: Header for the low-level DMA driver functions.
// =================================================================================================

#ifndef DMA_DRIVER_H
#define DMA_DRIVER_H

#include "hw_platform.h"

// --- Function Prototypes ---

/**
 * @brief Resets the DMA controller's interrupt status and re-enables UIO interrupts.
 * @param dma_regs Pointer to the mapped DMA controller registers.
 * @param uio_fd File descriptor for the DMA's UIO device.
 */
void dma_reset_interrupts(Dma_Regs_t* dma_regs, int uio_fd);

/**
 * @brief Forces the DMA controller to a stop state and performs a soft reset.
 * @param dma_regs Pointer to the mapped DMA controller registers.
 */
void force_dma_stop(Dma_Regs_t* dma_regs);

#endif // DMA_DRIVER_H
// =================================================================================================
// File: dma_driver.c
// Description: Implementation of the low-level DMA driver functions.
// =================================================================================================

#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include "dma_driver.h"

void dma_reset_interrupts(Dma_Regs_t* dma_regs, int uio_fd) {
    // Disable all interrupts by writing 0 to the mask register.
    dma_regs->INTR_0_MASK_REG = 0;
    // Clear any pending interrupt flags by writing 1s to the status register.
    dma_regs->INTR_0_STAT_REG = 0xFFFFFFFF;

    // Ensure the above writes complete before proceeding.
    __sync_synchronize();

    // The UIO framework requires a read to re-enable interrupts. This initial
    // read will consume any stale interrupt count from a previous run.
    uint32_t irq_count;
    lseek(uio_fd, 0, SEEK_SET); // Rewind to the beginning of the file
    read(uio_fd, &irq_count, sizeof(irq_count));
}

void force_dma_stop(Dma_Regs_t* dma_regs) {
    // De-assert all start signals
    dma_regs->START_OPERATION_REG = 0;
    // Perform a soft-reset sequence on the controller
    dma_regs->CONFIG_REG &= ~1;
    __sync_synchronize(); // Memory barrier
    dma_regs->CONFIG_REG |= 1;
    __sync_synchronize(); // Memory barrier
    printf("DMA Controller Reset.\n");
}



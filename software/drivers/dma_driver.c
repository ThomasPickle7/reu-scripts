#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include "dma_driver.h"

/**************************************************************************************************
 * @brief Forcibly stops all DMA operations by clearing descriptor configurations.
 *************************************************************************************************/
void dma_force_stop(Dma_Regs_t* dma_regs) {
    printf("  Forcing DMA stop...\n");
    for (int i = 0; i < 32; ++i) {
        dma_regs->DESCRIPTOR[i].CONFIG_REG = 0;
    }
    for (int i = 0; i < 4; ++i) {
        dma_regs->STREAM_ADDR_REG[i] = 0;
    }
    __sync_synchronize(); // Memory barrier to ensure writes are visible to hardware
}

/**************************************************************************************************
 * @brief Performs an exhaustive reset of the DMA interrupt state.
 *************************************************************************************************/
void dma_reset_interrupts(Dma_Regs_t* dma_regs, int dma_uio_fd) {
    printf("\n--- Exhaustive Interrupt Reset ---\n");
    
    // 1. Stop any ongoing DMA activity
    dma_force_stop(dma_regs);
    
    // 2. Mask all interrupts at the DMA controller level
    dma_regs->INTR_0_MASK_REG = 0;
    __sync_synchronize();
    
    // 3. Drain any pending interrupts from the UIO file descriptor
    uint32_t dummy;
    int flags = fcntl(dma_uio_fd, F_GETFL, 0);
    fcntl(dma_uio_fd, F_SETFL, flags | O_NONBLOCK);
    while(read(dma_uio_fd, &dummy, sizeof(dummy)) > 0); // Read until empty
    fcntl(dma_uio_fd, F_SETFL, flags); // Restore original flags
    
    // 4. Clear any status flags in the DMA controller
    dma_regs->INTR_0_CLEAR_REG = FDMA_IRQ_CLEAR_ALL;
    __sync_synchronize();
    
    // 5. Re-enable interrupt reporting for the UIO device
    uint32_t irq_enable = 1;
    write(dma_uio_fd, &irq_enable, sizeof(irq_enable));
    
    printf("--- Interrupt Reset Complete ---\n");
}

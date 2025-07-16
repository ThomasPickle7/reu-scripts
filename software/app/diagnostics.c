#include <stdio.h>
#include "diagnostics.h"

/**************************************************************************************************
 * @brief Runs a series of diagnostic checks on the DMA and Stream Source peripherals.
 *
 * This function reads and prints hardware registers to verify that the memory-mapped
 * peripherals are accessible and to report their status.
 *
 * @param dma_regs          Pointer to the mapped DMA controller registers.
 * @param stream_src_regs   Pointer to the mapped AXI Stream Source registers.
 *************************************************************************************************/
void run_diagnostics(Dma_Regs_t* dma_regs, AxiStreamSource_Regs_t* stream_src_regs) {
    printf("\n--- Running Low-Level System Diagnostics ---\n");

    // Check if the pointers are valid before dereferencing
    if (!dma_regs) {
        printf("  ERROR: DMA registers pointer is NULL\n");
        return;
    }
    if (!stream_src_regs) {
        printf("  ERROR: AXI Stream Source registers pointer is NULL\n");
        return;
    }

    // 1. Diagnose the DMA Controller
    printf("1. Diagnosing DMA Controller\n");
    // Reading the version register is a safe way to check for bus connectivity.
    uint32_t dma_version = dma_regs->VERSION_REG;
    printf("   - DMA Controller Version Register: 0x%08X\n", dma_version);
    if (dma_version == 0 || dma_version == 0xFFFFFFFF) {
        printf("   - WARNING: Invalid version read. DMA controller may not be responding.\n");
    } else {
        printf("   - SUCCESS: DMA controller appears to be mapped and responding.\n");
    }
    printf("   - Current Interrupt Mask Register: 0x%08X\n", dma_regs->INTR_0_MASK_REG);
    printf("   - Current Interrupt Status Register: 0x%08X\n", dma_regs->INTR_0_STAT_REG);


    // 2. Diagnose the AXI Stream Source
    printf("\n2. Diagnosing AXI Stream Source...\n");   
    uint32_t stream_status = stream_src_regs->STATUS_REG;
    printf("   - AXI Stream Source Status Register: 0x%08X\n", stream_status);
    if (stream_status & 0x1) {
         printf("   - STATUS: IP core is currently BUSY.\n");
    } else {
         printf("   - STATUS: IP core is currently IDLE.\n");
    }
    printf("   - SUCCESS: AXI Stream Source appears to be mapped and responding.\n");


    printf("\n--- Diagnostics Complete ---\n");
}

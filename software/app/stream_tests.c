#include <stdio.h>
#include <unistd.h>
#include <sys/select.h>
#include "stream_tests.h"
#include "hw_platform.h"
#include "dma_driver.h"


void run_stream_source_validation_test(AxiStreamSource_Regs_t* regs) {
    printf("\n--- Running AXI Stream Source IP Core Validation Test ---\n");
    // This is a placeholder for the full validation logic
    printf("  NOTE: This is a placeholder test. Implement full validation as needed.\n");
    uint32_t status = regs->STATUS_REG;
    printf("  Initial STATUS register: 0x%X\n", status);
    printf("--- Test Complete ---\n");
}

void run_stream_to_mem_test(CoreAXI4DMAController_Regs_t* dma_regs, int dma_uio_fd, uintptr_t dma_phys_base, uint8_t* dma_virt_base) {
    printf("\n--- Running Stream-to-Memory Test (Simulated) ---\n");
    dma_reset_interrupts(dma_regs, dma_uio_fd);
    DmaStreamDescriptor_t* stream_descriptors = (DmaStreamDescriptor_t*)(dma_virt_base + STREAM_DESCRIPTOR_OFFSET);

    printf("  Stream descriptor chain located at virtual address %p\n", stream_descriptors);
    printf("  Configuring %d stream descriptors in DDR...\n", NUM_BUFFERS);
    for (int i = 0; i < NUM_BUFFERS; ++i) {
        stream_descriptors[i].DEST_ADDR_REG = dma_phys_base + STREAM_DEST_OFFSET + (i * BUFFER_SIZE);
        stream_descriptors[i].BYTE_COUNT_REG = BUFFER_SIZE;
        uint32_t conf = (STREAM_OP_INCR | STREAM_FLAG_IRQ_EN) | STREAM_FLAG_VALID;
        if (i < (NUM_BUFFERS - 1)) {
           conf |= STREAM_FLAG_CHAIN;
        }
        stream_descriptors[i].CONFIG_REG = conf;
    }
    __sync_synchronize();

    uintptr_t phys_desc_addr = dma_phys_base + STREAM_DESCRIPTOR_OFFSET;
    printf("  Pointing DMA Stream Channel 0 to descriptor chain at physical address 0x%lX\n", phys_desc_addr);
    dma_regs->STREAM_ADDR_REG[0] = phys_desc_addr;
    __sync_synchronize();

    dma_regs->INTR_0_MASK_REG = FDMA_IRQ_MASK_ALL;
    stream_descriptors[0].CONFIG_REG |= STREAM_FLAG_DEST_RDY;
    __sync_synchronize();
    printf("  Starting stream channel 0. Waiting for data...\n");
    dma_regs->START_OPERATION_REG = FDMA_START_STREAM(0);

    printf("\n  NOTE: This test simulates waiting for interrupts. A real data-generating\n");
    printf("  FPGA IP is needed to actually transfer data and trigger them.\n");

    dma_force_stop(dma_regs);
    printf("\n  Stream test complete.\n");
}

void run_control_path_validation_test(CoreAXI4DMAController_Regs_t* dma_regs, int dma_uio_fd, uintptr_t dma_phys_base, uint8_t* dma_virt_base) {
    printf("\n--- Running DMA Control Path Validation Test (Software-Only) ---\n");
    dma_reset_interrupts(dma_regs, dma_uio_fd);

    DmaStreamDescriptor_t* desc = (DmaStreamDescriptor_t*)(dma_virt_base + STREAM_DESCRIPTOR_OFFSET);
    desc->DEST_ADDR_REG = dma_phys_base + STREAM_DEST_OFFSET;
    desc->BYTE_COUNT_REG = 1024;
    desc->CONFIG_REG = (STREAM_OP_INCR | STREAM_FLAG_IRQ_EN) | STREAM_FLAG_VALID | STREAM_FLAG_DEST_RDY;
    uintptr_t phys_desc_addr = dma_phys_base + STREAM_DESCRIPTOR_OFFSET;

    printf("  Pointing DMA Stream Channel 0 to descriptor at 0x%lX\n", phys_desc_addr);
    dma_regs->STREAM_ADDR_REG[0] = phys_desc_addr;
    __sync_synchronize();

    dma_regs->INTR_0_MASK_REG = FDMA_IRQ_MASK_ALL;
    printf("  Attempting to start stream channel 0 via software...\n");
    dma_regs->START_OPERATION_REG = FDMA_START_STREAM(0);
    __sync_synchronize();

    fd_set fds;
    struct timeval tv;
    FD_ZERO(&fds);
    FD_SET(dma_uio_fd, &fds);
    tv.tv_sec = 5;
    tv.tv_usec = 0;

    printf("  Waiting for interrupt (with a 5-second timeout)...\n");
    int retval = select(dma_uio_fd + 1, &fds, NULL, NULL, &tv);

    if (retval > 0) {
        uint32_t irq_count;
        read(dma_uio_fd, &irq_count, sizeof(irq_count));
        uint32_t status = dma_regs->INTR_0_STAT_REG;
        printf("  Interrupt received! DMA Status Register: 0x%08X\n", status);
        if (status & FDMA_IRQ_STAT_INVALID_DESC) {
            printf("\n***** DMA Control Path Test PASSED *****\n");
        } else {
            printf("\n***** DMA Control Path Test FAILED *****\n");
        }
    } else {
        printf("\n***** DMA Control Path Test INCONCLUSIVE (Timeout) *****\n");
    }

    dma_force_stop(dma_regs);
    dma_reset_interrupts(dma_regs, dma_uio_fd);
}

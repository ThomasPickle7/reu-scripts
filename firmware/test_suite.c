
// =================================================================================================
// File: test_suite.c
// Description: Implementation of the test and diagnostic functions.
// =================================================================================================

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "test_suite.h"
#include "app_config.h"
#include "dma_driver.h"

void run_axi_stream_source_test(
    Dma_Regs_t* dma_regs,
    AxiStreamSource_Regs_t* stream_src_regs,
    int dma_uio_fd,
    uintptr_t dma_phys_base,
    uint8_t* dma_virt_base)
{
    printf("\n--- Running Custom AXI Stream Source -> DDR Test ---\n");

    const size_t test_size = 4096; // Transfer 4KB of data
    int test_passed = 1;

    // --- Pre-Test State ---
    printf("  Initial DMA INTR_0_STAT_REG: 0x%08X\n", dma_regs->INTR_0_STAT_REG);
    printf("  Initial Stream Source STATUS_REG: 0x%08X\n", stream_src_regs->STATUS_REG);

    // 1. Reset DMA and interrupts to a clean state
    dma_reset_interrupts(dma_regs, dma_uio_fd);
    printf("  DMA interrupts reset.\n");

    // 2. Prepare the destination buffer in DDR memory
    uint8_t* virt_dest_buf = dma_virt_base + STREAM_DEST_OFFSET;
    uintptr_t phys_dest_buf = dma_phys_base + STREAM_DEST_OFFSET;
    memset(virt_dest_buf, 0, test_size);
    printf("  Destination DDR buffer prepared at virtual 0x%p / physical 0x%lX\n", virt_dest_buf, phys_dest_buf);

    // 3. Configure a single stream descriptor in DMA-accessible memory
    DmaStreamDescriptor_t* stream_descriptor = (DmaStreamDescriptor_t*)(dma_virt_base + STREAM_DESCRIPTOR_OFFSET);
    stream_descriptor->DEST_ADDR_REG  = phys_dest_buf;
    stream_descriptor->BYTE_COUNT_REG = test_size;
    stream_descriptor->CONFIG_REG = STREAM_OP_INCR | STREAM_FLAG_IRQ_EN | STREAM_FLAG_DEST_RDY | STREAM_FLAG_VALID;

    uintptr_t phys_desc_addr = dma_phys_base + STREAM_DESCRIPTOR_OFFSET;
    printf("  Stream descriptor configured at physical address 0x%lX\n", phys_desc_addr);
    printf("  Descriptor Contents: DEST_ADDR=0x%08X, BYTES=0x%X, CONFIG=0x%X\n",
           stream_descriptor->DEST_ADDR_REG, stream_descriptor->BYTE_COUNT_REG, stream_descriptor->CONFIG_REG);

    // 4. Point the DMA's stream channel to our descriptor
    dma_regs->STREAM_ADDR_REG[0] = phys_desc_addr;
    dma_regs->INTR_0_MASK_REG = FDMA_IRQ_MASK_STREAM_DONE(0); // Unmask interrupt for stream 0
    __sync_synchronize();
    printf("  DMA configured: STREAM_ADDR_REG[0]=0x%08X, INTR_0_MASK_REG=0x%08X\n",
           dma_regs->STREAM_ADDR_REG[0], dma_regs->INTR_0_MASK_REG);

    // 5. Start the DMA stream channel (it will now wait for the stream)
    dma_regs->START_OPERATION_REG = FDMA_START_STREAM(0);
    __sync_synchronize();
    printf("  DMA Stream Channel 0 started. Waiting for data...\n");
    printf("  DMA INTR_0_STAT_REG after start: 0x%08X\n", dma_regs->INTR_0_STAT_REG);

    // 6. Configure and start the AXI Stream Source module
    printf("  Configuring AXI Stream Source to send %zu bytes...\n", test_size);
    stream_src_regs->NUM_BYTES_REG = test_size;
    stream_src_regs->DEST_REG = 0; // TDEST value
    __sync_synchronize();

    printf("  Starting Stream Source IP...\n");
    stream_src_regs->CONTROL_REG = 1; // Assert start bit
    __sync_synchronize();
    stream_src_regs->CONTROL_REG = 0; // De-assert start (it's a pulse)
    printf("  AXI Stream Source started. Stream Source STATUS_REG: 0x%08X\n", stream_src_regs->STATUS_REG);

    // 7. Wait for the DMA completion interrupt
    uint32_t irq_count;
    printf("  Waiting for DMA completion interrupt... (If the program hangs here, the interrupt is not firing)\n");
    read(dma_uio_fd, &irq_count, sizeof(irq_count));
    uint32_t status = dma_regs->INTR_0_STAT_REG;
    printf("  Interrupt received! IRQ Count: %u, DMA Status Register: 0x%08X\n", irq_count, status);

    // 8. Verify the received data
    printf("  Verifying received data...\n");
    for (size_t i = 0; i < test_size / 4; ++i) {
        uint32_t expected_data = i; // The verilog module sends an incrementing pattern
        uint32_t actual_data = ((uint32_t*)virt_dest_buf)[i];
        if (expected_data != actual_data) {
            printf("  ERROR: Data mismatch at offset 0x%zX! Expected: 0x%08X, Got: 0x%08X\n",
                   i * 4, expected_data, actual_data);
            test_passed = 0;
            break;
        }
    }

    if (test_passed) {
        printf("\n***** AXI Stream Source Test PASSED *****\n");
    } else {
        printf("\n***** AXI Stream Source Test FAILED *****\n");
    }

    // Cleanup
    force_dma_stop(dma_regs);
    dma_reset_interrupts(dma_regs, dma_uio_fd);
}

void run_diagnostics(Dma_Regs_t* dma_regs, AxiStreamSource_Regs_t* stream_src_regs) {
    printf("\n--- Running Diagnostics ---\n");

    if (dma_regs) {
        printf("DMA Controller Registers:\n");
        printf("  ID_REG: 0x%08X\n", dma_regs->ID_REG);
        printf("  CONFIG_REG: 0x%08X\n", dma_regs->CONFIG_REG);
        printf("  INTR_0_STAT_REG: 0x%08X\n", dma_regs->INTR_0_STAT_REG);
        printf("  INTR_0_MASK_REG: 0x%08X\n", dma_regs->INTR_0_MASK_REG);
        printf("  STREAM_ADDR_REG[0]: 0x%08X\n", dma_regs->STREAM_ADDR_REG[0]);
    } else {
        printf("DMA Controller not mapped.\n");
    }

    if (stream_src_regs) {
        printf("AXI Stream Source Registers:\n");
        printf("  CONTROL_REG: 0x%08X\n", stream_src_regs->CONTROL_REG);
        printf("  STATUS_REG: 0x%08X\n", stream_src_regs->STATUS_REG);
        printf("  NUM_BYTES_REG: 0x%08X\n", stream_src_regs->NUM_BYTES_REG);
        printf("  DEST_REG: 0x%08X\n", stream_src_regs->DEST_REG);
    } else {
        printf("AXI Stream Source not mapped.\n");
    }
    printf("-------------------------\n");
}

void run_axi_lite_reg_test(AxiStreamSource_Regs_t* stream_src_regs) {
    printf("\n--- Running AXI-Lite Register Test ---\n");

    const uint32_t test_value = 0xDEADBEEF;
    int test_passed = 1;

    printf("  Writing 0x%08X to NUM_BYTES_REG (Offset 0x10)...\n", test_value);
    stream_src_regs->NUM_BYTES_REG = test_value;
    __sync_synchronize();

    printf("  Reading back from NUM_BYTES_REG...\n");
    uint32_t read_value = stream_src_regs->NUM_BYTES_REG;
    printf("  Read value: 0x%08X\n", read_value);

    if (read_value == test_value) {
        printf("  Read/Write test for NUM_BYTES_REG PASSED!\n");
    } else {
        printf("  ERROR: Read/Write test for NUM_BYTES_REG FAILED!\n");
        test_passed = 0;
    }
    
    // ... (rest of the function is identical) ...

    if(test_passed) {
        printf("\n***** Basic AXI-Lite communication appears to be WORKING. *****\n");
    } else {
        printf("\n***** Basic AXI-Lite communication FAILED. Check FPGA design. *****\n");
    }
}


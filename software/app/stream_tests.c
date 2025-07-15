#include <stdio.h>
#include <unistd.h>
#include <sys/select.h>
#include "stream_tests.h"
#include "hw_platform.h"
#include "dma_driver.h"

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

/**************************************************************************************************
 * @brief Runs a basic functionality test of the custom AXI Stream Source IP.
 *
 * This test configures the DMA to receive a stream and then commands the
 * AXI Stream Source to send a known data pattern. It then verifies if the
 * data was received correctly in DDR memory.
 *************************************************************************************************/
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

    // 1. Reset DMA and interrupts to a clean state
    dma_reset_interrupts(dma_regs, dma_uio_fd);

    // 2. Prepare the destination buffer in DDR memory
    uint8_t* virt_dest_buf = dma_virt_base + STREAM_DEST_OFFSET;
    uintptr_t phys_dest_buf = dma_phys_base + STREAM_DEST_OFFSET;
    memset(virt_dest_buf, 0, test_size); // Clear the destination buffer
    printf("  Destination DDR buffer prepared at physical address 0x%lX\n", phys_dest_buf);

    // 3. Configure a single stream descriptor in DMA-accessible memory
    DmaStreamDescriptor_t* stream_descriptor = (DmaStreamDescriptor_t*)(dma_virt_base + STREAM_DESCRIPTOR_OFFSET);
    stream_descriptor->DEST_ADDR_REG  = phys_dest_buf;
    stream_descriptor->BYTE_COUNT_REG = test_size;
    stream_descriptor->CONFIG_REG = STREAM_OP_INCR | STREAM_FLAG_IRQ_EN | STREAM_FLAG_DEST_RDY | STREAM_FLAG_VALID;
    
    uintptr_t phys_desc_addr = dma_phys_base + STREAM_DESCRIPTOR_OFFSET;
    printf("  Stream descriptor configured at physical address 0x%lX\n", phys_desc_addr);

    // 4. Point the DMA's stream channel to our descriptor
    dma_regs->STREAM_ADDR_REG[0] = phys_desc_addr;
    dma_regs->INTR_0_MASK_REG = FDMA_IRQ_MASK_ALL; // Unmask all interrupt types
    __sync_synchronize(); // Ensure descriptor and register writes are visible to hardware

    // 5. Start the DMA stream channel (it will now wait for the stream)
    dma_regs->START_OPERATION_REG = FDMA_START_STREAM(0);
    printf("  DMA Stream Channel 0 started. Waiting for data...\n");

    // 6. Configure and start the AXI Stream Source module
    printf("  Configuring AXI Stream Source to send %zu bytes...\n", test_size);
    stream_src_regs->NUM_BYTES_REG = test_size;
    stream_src_regs->DEST_REG = 0; // TDEST value
    stream_src_regs->CONTROL_REG = 1; // Assert start bit
    __sync_synchronize();
    printf("  AXI Stream Source started.\n");

    // 7. Wait for the DMA completion interrupt
    uint32_t irq_count;
    printf("  Waiting for DMA completion interrupt...\n");
    read(dma_uio_fd, &irq_count, sizeof(irq_count));
    uint32_t status = dma_regs->INTR_0_STAT_REG;
    printf("  Interrupt received! DMA Status Register: 0x%08X\n", status);
    
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
    stream_src_regs->CONTROL_REG = 0; // De-assert start
    dma_reset_interrupts(dma_regs, dma_uio_fd);
}

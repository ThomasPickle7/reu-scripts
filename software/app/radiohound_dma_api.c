#include <stdio.h>
#include <unistd.h>
#include "radiohound_dma_api.h"
#include "dma_driver.h"
#include "hw_platform.h"

// Helper function to generate test data
static void generate_test_data(uint8_t* buffer, size_t size, uint8_t seed) {
    printf("  Generating %zu bytes of test data with seed 0x%02X...\n", size, seed);
    for (size_t i = 0; i < size; ++i) {
        buffer[i] = (uint8_t)((i + seed) * 13 + ((i + seed) >> 8) * 7);
    }
}

// Helper function to verify data
static int verify_data_transfer(uint8_t* expected, uint8_t* actual, size_t size, int buffer_num) {
    printf("\n--- Verifying Buffer %d ---\n", buffer_num);
    size_t errors = 0;
    size_t first_error_offset = (size_t)-1;
    for (size_t i = 0; i < size; ++i) {
        if (expected[i] != actual[i]) {
            if (errors == 0) first_error_offset = i;
            errors++;
        }
    }
    double percentage = 100.0 * (double)(size - errors) / (double)size;
    printf("  Verification Result: %.2f%% matched. %zu bytes transferred, %zu errors found.\n", percentage, size, errors);
    if (errors > 0) {
        printf("  ERROR: First mismatch at offset 0x%zX! Expected: 0x%02X, Got: 0x%02X\n",
               first_error_offset, expected[first_error_offset], actual[first_error_offset]);
        return 0;
    } else {
        printf("  SUCCESS: Data integrity verified.\n");
        return 1;
    }
}


void rh_run_mem_test(Dma_Regs_t* dma_regs, int dma_uio_fd, uint64_t dma_phys_base, uint8_t* dma_virt_base) {
    printf("\n--- Running Memory-to-Memory Ping-Pong Test ---\n");
    dma_reset_interrupts(dma_regs, dma_uio_fd);

    uint8_t* virt_src_buf = dma_virt_base + PING_PONG_SRC_OFFSET;
    uint8_t* virt_dest_buf = dma_virt_base + PING_PONG_DEST_OFFSET;
    
    for(int i = 0; i < NUM_BUFFERS; ++i) { 
        generate_test_data(virt_src_buf + (i * BUFFER_SIZE), BUFFER_SIZE, i); 
    }

    printf("\n  Configuring %d internal descriptors for cyclic transfer...\n", NUM_BUFFERS);
    for (int i = 0; i < NUM_BUFFERS; ++i) {
        dma_regs->DESCRIPTOR[i].SOURCE_ADDR_REG = dma_phys_base + PING_PONG_SRC_OFFSET + (i * BUFFER_SIZE);
        dma_regs->DESCRIPTOR[i].DEST_ADDR_REG   = dma_phys_base + PING_PONG_DEST_OFFSET + (i * BUFFER_SIZE);
        dma_regs->DESCRIPTOR[i].BYTE_COUNT_REG  = BUFFER_SIZE;
        dma_regs->DESCRIPTOR[i].NEXT_DESC_ADDR_REG = (i + 1) % NUM_BUFFERS;
        dma_regs->DESCRIPTOR[i].CONFIG_REG = MEM_CONF_BASE | MEM_FLAG_SRC_RDY | MEM_FLAG_VALID;
    }
    __sync_synchronize();
    
    dma_regs->INTR_0_MASK_REG = FDMA_IRQ_MASK_ALL;
    printf("  Starting ping-pong transfer...\n");
    dma_regs->DESCRIPTOR[0].CONFIG_REG |= MEM_FLAG_DEST_RDY;
    __sync_synchronize();
    dma_regs->START_OPERATION_REG = FDMA_START_MEM(0);
    
    for (int i = 0; i < NUM_BUFFERS; ++i) { // A full cycle
        uint32_t irq_count;
        printf("  Waiting for interrupt %d of %d...\n", i + 1, NUM_BUFFERS);
        read(dma_uio_fd, &irq_count, sizeof(irq_count));
        uint32_t status = dma_regs->INTR_0_STAT_REG;
        uint32_t completed_desc = (status >> 4) & 0x3F;
        printf("  Interrupt for Descriptor %u received.\n", completed_desc);

        uint32_t next_desc_to_arm = (completed_desc + 1) % NUM_BUFFERS;
        dma_regs->DESCRIPTOR[next_desc_to_arm].CONFIG_REG |= (MEM_FLAG_DEST_RDY | MEM_FLAG_SRC_RDY);

        __sync_synchronize();
        dma_regs->INTR_0_CLEAR_REG = FDMA_IRQ_CLEAR_ALL;
        uint32_t irq_enable = 1;
        write(dma_uio_fd, &irq_enable, sizeof(irq_enable));
    }
    
    dma_force_stop(dma_regs);
    printf("\n  All transfers complete. Verifying data integrity...\n");
    int all_passed = 1;
    for (int i = 0; i < NUM_BUFFERS; i++) {
        if (!verify_data_transfer(virt_src_buf + (i * BUFFER_SIZE), virt_dest_buf + (i * BUFFER_SIZE), BUFFER_SIZE, i)) {
            all_passed = 0;
        }
    }
    if(all_passed) { printf("\n***** Mem-to-Mem Ping-Pong Test PASSED *****\n"); }
    else { printf("\n***** Mem-to-Mem Ping-Pong Test FAILED *****\n"); }
}

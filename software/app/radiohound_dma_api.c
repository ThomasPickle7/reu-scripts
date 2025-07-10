/**************************************************************************************************
 * @file radiohound_dma_api.c
 * @author Thomas Pickle
 * @brief High-level API for RadioHound DMA operations
 * @version 0.2
 * @date 2024-07-10
 *
 * @copyright Copyright (c) 2024
 *
 *************************************************************************************************/

#include "radiohound_dma_api.h"
#include "dma_driver.h"
#include "hw_platform.h"
#include <stdio.h> // For printf

// Global DMA instance handle
static dma_instance_t dma_inst;

/**************************************************************************************************
 * @brief Initializes the DMA system for RadioHound.
 *************************************************************************************************/
void rh_dma_init(void) {
    dma_init(&dma_inst, AXI_DMA_BASE_ADDR);
}

/**************************************************************************************************
 * @brief Generates predictable test data in a buffer.
 *************************************************************************************************/
void rh_generate_test_data(uint8_t* buffer, size_t size, uint8_t seed) {
    printf("  Generating %u bytes of test data with seed 0x%02X...\n", size, seed);
    for (size_t i = 0; i < size; ++i) {
        buffer[i] = (uint8_t)((i + seed) * 13 + ((i + seed) >> 8) * 7);
    }
}

/**************************************************************************************************
 * @brief Verifies the contents of a destination buffer against an expected source.
 *************************************************************************************************/
int rh_verify_data_transfer(uint8_t* expected, uint8_t* actual, size_t size, int buffer_num) {
    printf("\n--- Verifying Buffer %d ---\n", buffer_num);
    size_t errors = 0;
    size_t first_error_offset = (size_t)-1;

    for (size_t i = 0; i < size; ++i) {
        if (expected[i] != actual[i]) {
            if (errors == 0) {
                first_error_offset = i;
            }
            errors++;
        }
    }

    double percentage = 100.0 * (double)(size - errors) / (double)size;
    printf("  Verification Result: %.2f%% matched. %u bytes transferred, %u errors found.\n", percentage, size, errors);

    if (errors > 0) {
        printf("  ERROR: First mismatch at offset 0x%X! Expected: 0x%02X, Got: 0x%02X\n",
               first_error_offset, expected[first_error_offset], actual[first_error_offset]);
        return 0; // Failure
    } else {
        printf("  SUCCESS: Data integrity verified.\n");
        return 1; // Success
    }
}


/**************************************************************************************************
 * @brief Runs a full memory-to-memory ping-pong DMA test.
 *************************************************************************************************/
int rh_dma_run_m2m_ping_pong_test(int num_transfers) {
    printf("\n--- Running Memory-to-Memory Ping-Pong Test ---\n");
    dma_reset_interrupts(&dma_inst);

    // Get pointers to the physical buffer locations
    uint8_t* virt_src_base = (uint8_t*)(DMA_BUFFER_AREA_BASE + PING_PONG_SRC_OFFSET);
    uint8_t* virt_dest_base = (uint8_t*)(DMA_BUFFER_AREA_BASE + PING_PONG_DEST_OFFSET);
    uint32_t phys_src_base = DMA_BUFFER_AREA_BASE + PING_PONG_SRC_OFFSET;
    uint32_t phys_dest_base = DMA_BUFFER_AREA_BASE + PING_PONG_DEST_OFFSET;

    // 1. Generate test data for all source buffers
    for(int i = 0; i < NUM_BUFFERS; ++i) {
        rh_generate_test_data(virt_src_base + (i * BUFFER_SIZE), BUFFER_SIZE, i);
    }

    // 2. Configure all internal descriptors for a cyclic transfer
    printf("\n  Configuring %d internal descriptors for cyclic transfer...\n", NUM_BUFFERS);
    for (int i = 0; i < NUM_BUFFERS; ++i) {
        uint32_t src_addr = phys_src_base + (i * BUFFER_SIZE);
        uint32_t dest_addr = phys_dest_base + (i * BUFFER_SIZE);
        uint8_t next_desc = (i + 1) % NUM_BUFFERS;
        dma_configure_m2m_descriptor(&dma_inst, i, src_addr, dest_addr, BUFFER_SIZE, next_desc, 1);
    }
    __sync_synchronize();

    // 3. Enable interrupts and start the transfer loop
    dma_inst.regs->INTR_0_MASK_REG = FDMA_IRQ_MASK_ALL;
    printf("  Starting ping-pong transfer for %d buffers...\n", num_transfers);

    // Arm the first descriptor's source and destination
    dma_inst.regs->DESCRIPTOR[0].CONFIG_REG |= (MEM_FLAG_SRC_RDY | MEM_FLAG_DEST_RDY);
    __sync_synchronize();
    dma_start_channel(&dma_inst, 0);

    for (int i = 0; i < num_transfers; ++i) {
        printf("  Waiting for interrupt %d of %d...\n", i + 1, num_transfers);
        uint32_t status = dma_wait_for_interrupt(&dma_inst);
        uint32_t completed_desc = (status >> 4) & 0x3F; // Extract completed descriptor index
        printf("  Interrupt for Descriptor %u received.\n", completed_desc);

        // Re-arm the completed source buffer and arm the next destination buffer
        if (i < (num_transfers - 1)) {
            uint32_t next_desc_to_arm = (completed_desc + 1) % NUM_BUFFERS;
            dma_inst.regs->DESCRIPTOR[completed_desc].CONFIG_REG |= MEM_FLAG_SRC_RDY;
            dma_inst.regs->DESCRIPTOR[next_desc_to_arm].CONFIG_REG |= MEM_FLAG_DEST_RDY;
        } else {
            // On the last transfer, break the chain
            dma_inst.regs->DESCRIPTOR[completed_desc].CONFIG_REG &= ~MEM_FLAG_CHAIN;
        }
        __sync_synchronize();
        dma_clear_interrupt(&dma_inst, FDMA_IRQ_CLEAR_ALL);
    }

    dma_force_stop(&dma_inst);
    printf("\n  All transfers complete. Verifying data integrity...\n");

    // 4. Verify data
    int all_passed = 1;
    for (int i = 0; i < NUM_BUFFERS; i++) {
        if (!rh_verify_data_transfer(virt_src_base + (i * BUFFER_SIZE), virt_dest_base + (i * BUFFER_SIZE), BUFFER_SIZE, i)) {
            all_passed = 0;
        }
    }

    return all_passed ? 0 : -1; // Return 0 on success
}

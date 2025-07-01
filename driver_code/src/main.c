// SPDX-License-Identifier: MIT
/*
 * DMA test suite for PolarFire SoC, focused on AXI4-Stream to Memory.
 *
 * v22: Corrected Memory Mapping for Non-Cacheable Regions
 * - The 'msync failed: Invalid argument' error persisted because the target
 * DDR region (0xC8000000 onwards) is configured as NON-CACHEABLE memory.
 * - Cache synchronization operations (msync) are invalid on non-cacheable
 * memory, as there is no CPU cache to flush from or invalidate.
 * - The fix is to remove all calls to msync(). Coherency is guaranteed by
 * the hardware/kernel, as all accesses (CPU and DMA) to this region
 * bypass the cache and go directly to physical DDR.
 * - Simplified mmap() calls to reflect the actual size needed, as page-size
 * rounding for msync() is no longer required.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <errno.h>
#include <stdint.h>
#include <time.h>       // For high-resolution timing
#include "mpu_driver.h" // For MPU setup

/*
 * This struct represents the AXI4-Stream descriptor that resides
 * in system memory (DDR), not in the DMA controller's registers.
 */
typedef struct {
    volatile uint32_t CONFIG_REG;       // Offset +0x00
    volatile uint32_t BYTE_COUNT_REG;   // Offset +0x04
    volatile uint32_t DEST_ADDR_REG;    // Offset +0x08
} DmaStreamDescriptor_t;


/*
 * This struct directly mirrors the register layout of the CoreAXI4DMAController.
 * It is simplified to only include registers relevant to the stream test.
 */
typedef struct {
    volatile const uint32_t VERSION_REG;
    volatile uint32_t       START_OPERATION_REG;
    uint8_t                 _RESERVED1[0x10 - 0x08];
    volatile const uint32_t INTR_0_STAT_REG;
    volatile uint32_t       INTR_0_MASK_REG;
    volatile uint32_t       INTR_0_CLEAR_REG;
    uint8_t                 _RESERVED2[0x460 - 0x1C];
    volatile uint32_t       STREAM_ADDR_REG[4]; // Offsets 0x460, 0x464, 0x468, 0x46C
} CoreAXI4DMAController_Regs_t;

// Device tree names
#define UIO_DMA_DEVNAME         "dma-controller@60010000"

// Memory addresses and test parameters
#define DDR_BASE                0xc8000000UL
#define NUM_STREAM_BUFFERS      4
#define STREAM_BUFFER_SIZE      (1024 * 1024) // 1MB per buffer
#define STREAM_DEST_BASE        DDR_BASE
#define STREAM_DESCRIPTOR_BASE  (STREAM_DEST_BASE + (NUM_STREAM_BUFFERS * STREAM_BUFFER_SIZE))
#define NUM_STREAM_TRANSFERS    16 // Must be a multiple of NUM_STREAM_BUFFERS for this test

// --- DMA Configuration Bit Flags ---
#define STREAM_OP_INCR          (0b01)
#define STREAM_FLAG_DEST_RDY    (1U << 2)
#define STREAM_FLAG_VALID       (1U << 3)
#define STREAM_CONF_BASE        (STREAM_OP_INCR)

// DMA Control values
#define FDMA_START(n)           (1U << (n))
#define FDMA_IRQ_MASK_ALL       (0x0FU)
#define FDMA_IRQ_CLEAR_ALL      (0x0FU)

// Helper constants
#define SYSFS_PATH_LEN          (128)
#define ID_STR_LEN              (32)
#define UIO_DEVICE_PATH_LEN     (32)
#define NUM_UIO_DEVICES         (32)
#define MAP_SIZE                4096UL

// --- Helper Functions ---

static int get_uio_device_number(const char *id) {
    FILE *fp; int i; char file_id[ID_STR_LEN]; char sysfs_path[SYSFS_PATH_LEN];
    for (i = 0; i < NUM_UIO_DEVICES; i++) {
        snprintf(sysfs_path, SYSFS_PATH_LEN, "/sys/class/uio/uio%d/name", i);
        fp = fopen(sysfs_path, "r"); if (fp == NULL) break;
        fscanf(fp, "%31s", file_id); fclose(fp);
        if (strncmp(file_id, id, strlen(id)) == 0) return i;
    }
    return -1;
}

void reset_interrupt_state(CoreAXI4DMAController_Regs_t* dma_regs, int dma_uio_fd) {
    // Disable and clear any pending hardware interrupts in the DMA controller
    dma_regs->INTR_0_MASK_REG = 0;
    dma_regs->INTR_0_CLEAR_REG = FDMA_IRQ_CLEAR_ALL;

    // Perform a non-blocking read to clear any pending interrupt count in the UIO driver.
    // This prevents an immediate return from the first blocking read() call.
    uint32_t dummy_irq_count;
    int flags = fcntl(dma_uio_fd, F_GETFL, 0);
    fcntl(dma_uio_fd, F_SETFL, flags | O_NONBLOCK);
    read(dma_uio_fd, &dummy_irq_count, sizeof(dummy_irq_count));
    fcntl(dma_uio_fd, F_SETFL, flags); // Restore original flags

    // Re-enable interrupt generation for the UIO device
    uint32_t irq_enable = 1;
    write(dma_uio_fd, &irq_enable, sizeof(irq_enable));
}

void generate_test_data(uint8_t* buffer, size_t size, uint8_t seed) {
    printf("  Generating %zu bytes of reference data with seed 0x%02X...\n", size, seed);
    for (size_t i = 0; i < size; ++i) {
        buffer[i] = (uint8_t)((i + seed) * 13 + ((i + seed) >> 8) * 7);
    }
}

int verify_stream_data(uint8_t* actual, size_t size, int transfer_num) {
    printf("\n--- Verifying transfer %d ---\n", transfer_num);
    
    // Allocate a temporary buffer for the expected data
    uint8_t* expected = malloc(size);
    if (!expected) {
        printf("  ERROR: Failed to allocate memory for verification buffer.\n");
        return 0;
    }
    
    // Generate the expected data pattern based on the transfer number seed
    generate_test_data(expected, size, (uint8_t)transfer_num);

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
    printf("  Verification Result: %.2f%% matched. %zu bytes transferred, %zu errors found.\n", percentage, size, errors);
    
    printf("  First 8 bytes received: ");
    for(int k=0; k<8; ++k) {
        printf("%02X ", actual[k]);
    }
    printf("\n");

    int success = 1;
    if (errors > 0) {
        printf("  ERROR: First mismatch at offset 0x%zX! Expected: 0x%02X, Got: 0x%02X\n", 
               first_error_offset, expected[first_error_offset], actual[first_error_offset]);
        success = 0;
    } else {
        printf("  SUCCESS: Data integrity verified for this transfer.\n");
    }
    
    free(expected);
    return success;
}


void run_stream_ping_pong(CoreAXI4DMAController_Regs_t* dma_regs, int dma_uio_fd, int mem_fd) {
    printf("\n--- Running AXI4-Stream to Memory Ping-Pong Test ---\n");

    reset_interrupt_state(dma_regs, dma_uio_fd);

    DmaStreamDescriptor_t *desc_region_vaddr = NULL;
    uint8_t *dest_bufs[NUM_STREAM_BUFFERS];
    
    // Since the memory is non-cacheable, we don't need to page-align the mmap length for msync.
    // We can just map the actual size needed. The kernel handles page granularity internally.
    size_t actual_desc_size = NUM_STREAM_BUFFERS * sizeof(DmaStreamDescriptor_t);

    desc_region_vaddr = mmap(NULL, actual_desc_size, PROT_READ | PROT_WRITE, MAP_SHARED, mem_fd, STREAM_DESCRIPTOR_BASE);
    if (desc_region_vaddr == MAP_FAILED) {
        perror("Failed to mmap stream descriptor region");
        return;
    }

    printf("  Mapping %d destination buffers and preparing descriptors in DDR...\n", NUM_STREAM_BUFFERS);
    for (int i = 0; i < NUM_STREAM_BUFFERS; ++i) {
        dest_bufs[i] = mmap(NULL, STREAM_BUFFER_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, mem_fd, STREAM_DEST_BASE + (i * STREAM_BUFFER_SIZE));
        if (dest_bufs[i] == MAP_FAILED) { 
            perror("Failed to mmap stream destination buffer"); 
            munmap(desc_region_vaddr, actual_desc_size); 
            // Unmap any previously mapped buffers
            for(int j = 0; j < i; j++) munmap(dest_bufs[j], STREAM_BUFFER_SIZE);
            return; 
        }
        memset(dest_bufs[i], 0, STREAM_BUFFER_SIZE); // Clear destination buffers

        DmaStreamDescriptor_t* current_desc = desc_region_vaddr + i;
        current_desc->DEST_ADDR_REG = STREAM_DEST_BASE + (i * STREAM_BUFFER_SIZE);
        current_desc->BYTE_COUNT_REG = STREAM_BUFFER_SIZE;
        current_desc->CONFIG_REG = STREAM_CONF_BASE | STREAM_FLAG_VALID | STREAM_FLAG_DEST_RDY;
    }
    
    // msync() is not needed because the memory region is non-cacheable.
    // Writes from the CPU go directly to DDR.

    dma_regs->INTR_0_MASK_REG = FDMA_IRQ_MASK_ALL;
    int next_dma_desc_idx = 0;
    int transfers_completed = 0;

    printf("\n*****************************************************************\n");
    printf("  System configured for continuous stream reception.\n");
    printf("  Please initiate the AXI4-Stream transfer from your hardware now.\n");
    printf("  Expecting %d transfers of %ld KB each.\n", NUM_STREAM_TRANSFERS, STREAM_BUFFER_SIZE / 1024);
    printf("*****************************************************************\n\n");

    // Point the DMA controller to the first descriptor in DDR
    dma_regs->STREAM_ADDR_REG[0] = STREAM_DESCRIPTOR_BASE + (next_dma_desc_idx * sizeof(DmaStreamDescriptor_t));
    
    while(transfers_completed < NUM_STREAM_TRANSFERS) {
        uint32_t irq_count;
        printf("Waiting for interrupt for transfer %d (buffer %d)...\n", transfers_completed, next_dma_desc_idx);
        
        // Block until the UIO interrupt is received
        ssize_t ret = read(dma_uio_fd, &irq_count, sizeof(irq_count));
        if (ret != sizeof(irq_count)) {
            perror("Interrupt read failed");
            break;
        }

        int completed_idx = next_dma_desc_idx;
        printf("  Interrupt received for Buffer %d.\n", completed_idx);
        
        dma_regs->INTR_0_CLEAR_REG = FDMA_IRQ_CLEAR_ALL;
        
        // Re-arm for the next transfer
        if (transfers_completed < NUM_STREAM_TRANSFERS - 1) {
            next_dma_desc_idx = (next_dma_desc_idx + 1) % NUM_STREAM_BUFFERS;
            dma_regs->STREAM_ADDR_REG[0] = STREAM_DESCRIPTOR_BASE + (next_dma_desc_idx * sizeof(DmaStreamDescriptor_t));
            
            // Re-enable the UIO interrupt so we can wait for the next one
            uint32_t irq_enable = 1;
            write(dma_uio_fd, &irq_enable, sizeof(irq_enable));
        }
        transfers_completed++;
    }

    // Stop the DMA controller from generating more interrupts
    dma_regs->INTR_0_MASK_REG = 0;
    printf("\nAll %d transfers complete. Verifying data integrity...\n", NUM_STREAM_TRANSFERS);

    // --- Verification Step ---
    // msync() is not needed before verification because the memory is non-cacheable.
    // The CPU will read the fresh data directly from DDR.
    int all_passed = 1;
    for (int i = 0; i < NUM_STREAM_BUFFERS; i++) {
        // The test assumes the N transfers fill the buffers cyclically.
        // We verify the data from the LAST time each buffer was filled.
        int last_transfer_for_this_buffer = (NUM_STREAM_TRANSFERS - NUM_STREAM_BUFFERS) + i;
        if (!verify_stream_data(dest_bufs[i], STREAM_BUFFER_SIZE, last_transfer_for_this_buffer)) {
            all_passed = 0;
        }
    }

    if(all_passed) {
        printf("\n***** AXI-Stream Ping-Pong Test PASSED *****\n");
    } else {
        printf("\n***** AXI-Stream Ping-Pong Test FAILED *****\n");
    }

// Cleanup is handled by the regular return path
    munmap(desc_region_vaddr, actual_desc_size);
    for (int i = 0; i < NUM_STREAM_BUFFERS; ++i) {
        if(dest_bufs[i] != MAP_FAILED) munmap(dest_bufs[i], STREAM_BUFFER_SIZE);
    }
}

int main(void) {
    int dma_uio_fd = -1, mem_fd = -1, uio_num;
    CoreAXI4DMAController_Regs_t *dma_regs = NULL;

    printf("--- PolarFire SoC AXI4-Stream DMA Test Application ---\n");

    if (!MPU_Configure_FIC0()) {
        fprintf(stderr, "Fatal: Could not configure MPU. Halting.\n");
        return 1;
    }

    uio_num = get_uio_device_number(UIO_DMA_DEVNAME);
    if (uio_num < 0) {
        fprintf(stderr, "Fatal: Could not find UIO device for %s.\n", UIO_DMA_DEVNAME);
        return 1;
    }
    
    char uio_dev_path[UIO_DEVICE_PATH_LEN];
    snprintf(uio_dev_path, UIO_DEVICE_PATH_LEN, "/dev/uio%d", uio_num);
    dma_uio_fd = open(uio_dev_path, O_RDWR);
    if (dma_uio_fd < 0) { perror("Fatal: Failed to open UIO device file"); return 1; }

    dma_regs = mmap(NULL, MAP_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, dma_uio_fd, 0);
    if (dma_regs == MAP_FAILED) { perror("Fatal: Failed to mmap UIO device"); close(dma_uio_fd); return 1; }

    // Open /dev/mem WITHOUT O_SYNC.
    mem_fd = open("/dev/mem", O_RDWR);
    if (mem_fd < 0) { perror("Fatal: Failed to open /dev/mem"); munmap(dma_regs, MAP_SIZE); close(dma_uio_fd); return 1; }

    printf("Reading DMA Controller Version: 0x%08X\n", dma_regs->VERSION_REG);

    run_stream_ping_pong(dma_regs, dma_uio_fd, mem_fd);

    munmap(dma_regs, MAP_SIZE);
    close(dma_uio_fd);
    close(mem_fd);
    printf("\nExiting.\n");

    return 0;
}

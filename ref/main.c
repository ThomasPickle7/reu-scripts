// SPDX-License-Identifier: MIT
/*
 * Combined and revised DMA loopback/throughput test for PolarFire SoC.
 * This version uses the UIO framework and implements a chained descriptor
 * throughput test with pre-flight configuration verification.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <errno.h>
#include <stdint.h>
#include <time.h>      // For high-resolution timing
#include "mpu_driver.h" // Assuming this is still needed for MPU setup

/*
 * This struct accurately represents a single DMA descriptor block in hardware,
 * including padding, allowing for easy array-based access.
 */
typedef struct {
    volatile uint32_t CONFIG_REG;           // Offset +0x00
    volatile uint32_t BYTE_COUNT_REG;       // Offset +0x04
    volatile uint32_t SOURCE_ADDR_REG;      // Offset +0x08
    volatile uint32_t DEST_ADDR_REG;        // Offset +0x0C
    volatile uint32_t NEXT_DESC_ADDR_REG;   // Offset +0x10
    uint8_t           _RESERVED[0x20 - 0x14]; // Pad to 32 bytes total
} DmaDescriptorBlock_t;

/*
 * This struct directly mirrors the register layout of the CoreAXI4DMAController
 * using the descriptor block structure defined above.
 */
typedef struct {
    volatile const uint32_t VERSION_REG;
    volatile uint32_t       START_OPERATION_REG;
    uint8_t                 _RESERVED1[0x10 - 0x08];
    volatile const uint32_t INTR_0_STAT_REG;
    volatile uint32_t       INTR_0_MASK_REG;
    volatile uint32_t       INTR_0_CLEAR_REG;
    uint8_t                 _RESERVED2[0x60 - 0x1C];
    DmaDescriptorBlock_t    DESCRIPTOR[4];
} CoreAXI4DMAController_Regs_t;

// Device tree names
#define UIO_DMA_DEVNAME         "dma-controller@60010000"
#define UIO_LSRAM_DEVNAME       "fpga_lsram"

// Memory addresses
#define LSRAM_BASE              0x60000000UL
#define DDR_BUFFER_BASE         0xc8000000UL

// Test parameters
#define LOOPBACK_BUFFER_SIZE    4096
#define NUM_CHAINED_DESCS       4
#define SINGLE_DESC_TRANSFER_SIZE (1024 * 1024) // 1MB per descriptor
#define TOTAL_CHAINED_TRANSFER_SIZE (NUM_CHAINED_DESCS * SINGLE_DESC_TRANSFER_SIZE)

// DMA Configuration Bit Flags
#define FLAG_CHAIN              (1U << 10)
#define FLAG_IRQ_ON_PROCESS     (1U << 12)
#define FLAG_VALID              (1U << 15)
#define FLAG_SRC_RDY            (1U << 13)
#define FLAG_DEST_RDY           (1U << 14)
#define OP_INCR                 (0b01)

// Base configuration for an incrementing transfer (without the VALID bit)
#define BASE_CONF               ((OP_INCR << 2) | OP_INCR | FLAG_SRC_RDY | FLAG_DEST_RDY)

// DMA Control values
#define FDMA_START              (1U << 0) // Start with descriptor 0
#define FDMA_IRQ_MASK           (1U << 0) // Unmask completion interrupt
#define FDMA_IRQ_CLEAR          (1U << 0) // Clear completion interrupt

// Helper constants
#define SYSFS_PATH_LEN          (128)
#define ID_STR_LEN              (32)
#define UIO_DEVICE_PATH_LEN     (32)
#define NUM_UIO_DEVICES         (32)
#define MAP_SIZE                4096UL

static int get_uio_device_number(const char *id) {
    FILE *fp;
    int i;
    char file_id[ID_STR_LEN];
    char sysfs_path[SYSFS_PATH_LEN];

    for (i = 0; i < NUM_UIO_DEVICES; i++) {
        snprintf(sysfs_path, SYSFS_PATH_LEN, "/sys/class/uio/uio%d/name", i);
        fp = fopen(sysfs_path, "r");
        if (fp == NULL) break;
        fscanf(fp, "%31s", file_id);
        fclose(fp);
        if (strncmp(file_id, id, strlen(id)) == 0) return i;
    }
    return -1;
}

void run_loopback_test(CoreAXI4DMAController_Regs_t* dma_regs, int dma_uio_fd, int mem_fd) {
    uint8_t *src_buf = NULL, *dest_buf = NULL;
    
    printf("\n--- Running Memory-to-Memory Loopback Test ---\n");

    src_buf = mmap(NULL, LOOPBACK_BUFFER_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, mem_fd, DDR_BUFFER_BASE);
    dest_buf = mmap(NULL, LOOPBACK_BUFFER_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, mem_fd, DDR_BUFFER_BASE + LOOPBACK_BUFFER_SIZE);
    if (src_buf == MAP_FAILED || dest_buf == MAP_FAILED) {
        perror("Failed to mmap DDR buffers for loopback"); return;
    }

    for (int i = 0; i < LOOPBACK_BUFFER_SIZE; i++) src_buf[i] = (uint8_t)(i % 256);
    memset(dest_buf, 0, LOOPBACK_BUFFER_SIZE);

    dma_regs->DESCRIPTOR[0].SOURCE_ADDR_REG = DDR_BUFFER_BASE;
    dma_regs->DESCRIPTOR[0].DEST_ADDR_REG = DDR_BUFFER_BASE + LOOPBACK_BUFFER_SIZE;
    dma_regs->DESCRIPTOR[0].BYTE_COUNT_REG = LOOPBACK_BUFFER_SIZE;
    dma_regs->DESCRIPTOR[0].NEXT_DESC_ADDR_REG = 0;
    
    // Step 1: Write config without the VALID bit
    dma_regs->DESCRIPTOR[0].CONFIG_REG = BASE_CONF | FLAG_IRQ_ON_PROCESS;
    // Step 2: Write again with the VALID bit to arm the descriptor
    dma_regs->DESCRIPTOR[0].CONFIG_REG |= FLAG_VALID;
    
    dma_regs->INTR_0_MASK_REG = FDMA_IRQ_MASK;
    dma_regs->START_OPERATION_REG = FDMA_START;

    printf("Waiting for DMA completion interrupt...\n");
    uint32_t irq_count;
    read(dma_uio_fd, &irq_count, sizeof(irq_count));
    printf("Interrupt received!\n");
    dma_regs->INTR_0_CLEAR_REG = FDMA_IRQ_CLEAR;

    if (memcmp(src_buf, dest_buf, LOOPBACK_BUFFER_SIZE) == 0) printf("***** Loopback Test PASSED *****\n");
    else printf("***** Loopback Test FAILED *****\n");

    munmap(src_buf, LOOPBACK_BUFFER_SIZE); munmap(dest_buf, LOOPBACK_BUFFER_SIZE);
}

void run_chained_throughput_test(CoreAXI4DMAController_Regs_t* dma_regs, int dma_uio_fd, int mem_fd) {
    printf("\n--- Running Chained LSRAM-to-DDR Throughput Test ---\n");
    
    uint32_t intended_config[NUM_CHAINED_DESCS];
    uint32_t intended_next_desc[NUM_CHAINED_DESCS];

    // --- Step 1: Configure all descriptors without the VALID bit ---
    printf("Configuring %d descriptors in a linear chain (0->1->2->3)...\n", NUM_CHAINED_DESCS);
    for (int i = 0; i < NUM_CHAINED_DESCS; i++) {
        dma_regs->DESCRIPTOR[i].SOURCE_ADDR_REG = LSRAM_BASE;
        dma_regs->DESCRIPTOR[i].DEST_ADDR_REG = DDR_BUFFER_BASE + (i * SINGLE_DESC_TRANSFER_SIZE);
        dma_regs->DESCRIPTOR[i].BYTE_COUNT_REG = SINGLE_DESC_TRANSFER_SIZE;
        
        uint32_t temp_config = BASE_CONF;
        if (i < NUM_CHAINED_DESCS - 1) {
            temp_config |= FLAG_CHAIN;
            intended_next_desc[i] = i + 1;
        } else {
            temp_config |= FLAG_IRQ_ON_PROCESS;
            intended_next_desc[i] = 0;
        }
        
        intended_config[i] = temp_config | FLAG_VALID; // Store the final intended value for verification
        
        dma_regs->DESCRIPTOR[i].CONFIG_REG = temp_config; // Write config without VALID bit
        dma_regs->DESCRIPTOR[i].NEXT_DESC_ADDR_REG = intended_next_desc[i];
    }
    
    // --- Step 2: Arm all descriptors by setting the VALID bit ---
    printf("Arming descriptors by setting the VALID bit...\n");
    for (int i = 0; i < NUM_CHAINED_DESCS; i++) {
        dma_regs->DESCRIPTOR[i].CONFIG_REG |= FLAG_VALID;
    }
    
    dma_regs->INTR_0_MASK_REG = FDMA_IRQ_MASK;
    
    // --- Step 3: Read back and verify the configuration ---
    printf("\n--- Verifying Descriptor Configuration in Hardware ---\n");
    printf("Desc | Intended Config | Actual Config   | Match | Intended Next | Actual Next | Match\n");
    printf("-----|-----------------|-----------------|-------|---------------|-------------|-------\n");

    int config_ok = 1;
    for (int i = 0; i < NUM_CHAINED_DESCS; i++) {
        uint32_t actual_config = dma_regs->DESCRIPTOR[i].CONFIG_REG;
        uint32_t actual_next = dma_regs->DESCRIPTOR[i].NEXT_DESC_ADDR_REG;
        int config_match = (intended_config[i] == actual_config);
        int next_match = (intended_next_desc[i] == actual_next);
        
        printf("  %d  | 0x%08X      | 0x%08X      |  %s  |       %d       |      %d      |  %s\n",
            i, intended_config[i], actual_config, config_match ? "OK" : "FAIL",
            intended_next_desc[i], actual_next, next_match ? "OK" : "FAIL");

        if (!config_match || !next_match) config_ok = 0;
    }

    if (!config_ok) {
        printf("\nERROR: Hardware configuration does not match intended values. Aborting test.\n");
        return;
    }
    printf("Descriptor configuration verified successfully.\n");
    
    // --- Step 4: Run the transfer and time it ---
    printf("\nPerforming single kick-off for %dMB transfer...\n", TOTAL_CHAINED_TRANSFER_SIZE / (1024 * 1024));
    
    struct timespec start_time, end_time;
    clock_gettime(CLOCK_MONOTONIC, &start_time);

    dma_regs->START_OPERATION_REG = FDMA_START;
    
    uint32_t irq_count;
    read(dma_uio_fd, &irq_count, sizeof(irq_count));

    clock_gettime(CLOCK_MONOTONIC, &end_time);
    
    dma_regs->INTR_0_CLEAR_REG = FDMA_IRQ_CLEAR;
    printf("Final interrupt received and cleared.\n");

    double elapsed_time = (end_time.tv_sec - start_time.tv_sec) + (end_time.tv_nsec - start_time.tv_nsec) / 1e9;
    double throughput = (double)TOTAL_CHAINED_TRANSFER_SIZE / elapsed_time / (1024.0 * 1024.0);

    printf("\n***** Chained Throughput Test Complete *****\n");
    printf("Transferred %d MB in %.4f seconds.\n", TOTAL_CHAINED_TRANSFER_SIZE / (1024*1024), elapsed_time);
    printf("Calculated Throughput: %.2f MB/s\n", throughput);
    printf("******************************************\n");
}

int main(void) {
    int dma_uio_fd = -1, mem_fd = -1, uio_num;
    CoreAXI4DMAController_Regs_t *dma_regs = NULL;
    char cmd;

    printf("--- PolarFire SoC DMA Test Application ---\n");

    if (!MPU_Configure_FIC0()) {
        fprintf(stderr, "Fatal: Could not configure MPU. Halting.\n"); return 1;
    }

    uio_num = get_uio_device_number(UIO_DMA_DEVNAME);
    if (uio_num < 0) {
        fprintf(stderr, "Fatal: Could not find UIO device for %s.\n", UIO_DMA_DEVNAME); return 1;
    }
    
    char uio_dev_path[UIO_DEVICE_PATH_LEN];
    snprintf(uio_dev_path, UIO_DEVICE_PATH_LEN, "/dev/uio%d", uio_num);
    dma_uio_fd = open(uio_dev_path, O_RDWR);
    if (dma_uio_fd < 0) { perror("Fatal: Failed to open UIO device file"); return 1; }

    dma_regs = mmap(NULL, MAP_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, dma_uio_fd, 0);
    if (dma_regs == MAP_FAILED) { perror("Fatal: Failed to mmap UIO device"); close(dma_uio_fd); return 1; }

    mem_fd = open("/dev/mem", O_RDWR | O_SYNC);
    if (mem_fd < 0) { perror("Fatal: Failed to open /dev/mem"); munmap(dma_regs, MAP_SIZE); close(dma_uio_fd); return 1; }

    printf("Reading DMA Controller Version: 0x%08X\n", dma_regs->VERSION_REG);

    uint32_t irq_enable = 1;
    write(dma_uio_fd, &irq_enable, sizeof(irq_enable));

    while(1){
        printf("\n# Choose one of the following options:\n");
        printf("  1 - Run Memory-to-Memory Loopback Test\n");
        printf("  2 - Run Chained LSRAM-to-DDR Throughput Test\n");
        printf("  3 - Exit\n> ");
        
        scanf(" %c", &cmd);

        if (cmd == '1') {
            run_loopback_test(dma_regs, dma_uio_fd, mem_fd);
        } else if (cmd == '2') {
            run_chained_throughput_test(dma_regs, dma_uio_fd, mem_fd);
        } else if (cmd == '3' || cmd == 'q') {
            break;
        } else {
            printf("Invalid option.\n");
        }
    }

    // --- Cleanup ---
    munmap(dma_regs, MAP_SIZE); close(dma_uio_fd); close(mem_fd);
    printf("\nExiting.\n");

    return 0;
}

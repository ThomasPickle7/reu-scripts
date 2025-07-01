// SPDX-License-Identifier: MIT
/*
 * DMA test suite for PolarFire SoC, focused on AXI4-Stream to Memory.
 *
 * v25: Added Extensive Debugging Printouts
 * - Added a new function, print_debug_state, to be called immediately
 * before the application waits for the DMA interrupt.
 * - This function reads back and prints the state of:
 * 1. The AXI Stream Generator's control registers, to verify that the
 * writes to start the stream were successful.
 * 2. The CoreAXI4DMAController's key registers (interrupt mask, stream
 * descriptor address), to verify the receiver is configured correctly.
 * 3. The first stream descriptor in DDR memory, to verify it was
 * written correctly by the CPU.
 * - This provides a complete snapshot of the hardware state to diagnose the hang.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <errno.h>
#include <stdint.h>
#include <time.h>
#include "mpu_driver.h"

// --- Peripheral Definitions ---

/*
 * This struct directly mirrors the register layout of the CoreAXI4DMAController.
 */
typedef struct {
    volatile const uint32_t VERSION_REG;
    volatile uint32_t       START_OPERATION_REG;
    uint8_t                 _RESERVED1[0x10 - 0x08];
    volatile const uint32_t INTR_0_STAT_REG;
    volatile uint32_t       INTR_0_MASK_REG;
    volatile uint32_t       INTR_0_CLEAR_REG;
    uint8_t                 _RESERVED2[0x460 - 0x1C];
    volatile uint32_t       STREAM_ADDR_REG[4];
} CoreAXI4DMAController_Regs_t;

/*
 * This struct represents the AXI4-Stream descriptor that resides in system memory.
 */
typedef struct {
    volatile uint32_t CONFIG_REG;
    volatile uint32_t BYTE_COUNT_REG;
    volatile uint32_t DEST_ADDR_REG;
} DmaStreamDescriptor_t;

/*
 * This struct mirrors the APB registers of the AXI4_STREAM_DATA_GENERATOR
 */
typedef struct {
    volatile uint32_t TRANS_SIZE_REG;
    volatile uint32_t START_REG;
    volatile uint32_t RESET_GENERATOR_REG;
} AxiStreamGen_Regs_t;


// --- Device and Memory Configuration ---
#define UIO_DMA_DEVNAME         "dma-controller@60010000"

// IMPORTANT: You must verify this address from your FPGA design's memory map.
#define AXI_STREAM_GEN_BASE     0x60020000UL

#define DDR_BASE                0xc8000000UL
#define NUM_STREAM_BUFFERS      4
#define STREAM_BUFFER_SIZE      (1024 * 1024)
#define STREAM_DEST_BASE        DDR_BASE
#define STREAM_DESCRIPTOR_BASE  (STREAM_DEST_BASE + (NUM_STREAM_BUFFERS * STREAM_BUFFER_SIZE))
#define NUM_STREAM_TRANSFERS    16

// --- Bit Flags & Constants ---
#define STREAM_OP_INCR          (0b01)
#define STREAM_FLAG_DEST_RDY    (1U << 2)
#define STREAM_FLAG_VALID       (1U << 3)
#define STREAM_CONF_BASE        (STREAM_OP_INCR | STREAM_FLAG_VALID)

#define FDMA_IRQ_MASK_ALL       (0x0FU)
#define FDMA_IRQ_CLEAR_ALL      (0x0FU)

#define SYSFS_PATH_LEN          (128)
#define ID_STR_LEN              (32)
#define UIO_DEVICE_PATH_LEN     (32)
#define NUM_UIO_DEVICES         (32)
#define MAP_SIZE                4096UL

// --- Helper Functions (declarations) ---
static int get_uio_device_number(const char *id);
void reset_interrupt_state(CoreAXI4DMAController_Regs_t* dma_regs, int dma_uio_fd);
int verify_stream_data(uint8_t* actual, size_t size, int transfer_num);
void print_debug_state(CoreAXI4DMAController_Regs_t* dma_regs, AxiStreamGen_Regs_t* stream_gen, DmaStreamDescriptor_t* desc);


void run_stream_ping_pong(CoreAXI4DMAController_Regs_t* dma_regs, int dma_uio_fd, int mem_fd) {
    printf("\n--- Running AXI4-Stream to Memory Ping-Pong Test ---\n");

    reset_interrupt_state(dma_regs, dma_uio_fd);

    DmaStreamDescriptor_t *desc_region_vaddr = NULL;
    AxiStreamGen_Regs_t *stream_gen_vaddr = NULL;
    void* stream_gen_map_base = NULL;
    uint8_t *dest_bufs[NUM_STREAM_BUFFERS] = {NULL};
    
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
            goto cleanup;
        }
        memset(dest_bufs[i], 0, STREAM_BUFFER_SIZE);

        DmaStreamDescriptor_t* current_desc = desc_region_vaddr + i;
        current_desc->DEST_ADDR_REG = STREAM_DEST_BASE + (i * STREAM_BUFFER_SIZE);
        current_desc->BYTE_COUNT_REG = STREAM_BUFFER_SIZE;
        current_desc->CONFIG_REG = STREAM_CONF_BASE | STREAM_FLAG_DEST_RDY;
    }

    // --- Start the Hardware Stream Generator ---
    printf("  Configuring and starting the hardware AXI4-Stream generator...\n");
    stream_gen_map_base = mmap(NULL, MAP_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, mem_fd, AXI_STREAM_GEN_BASE & ~(MAP_SIZE - 1));
    if (stream_gen_map_base == MAP_FAILED) {
        perror("Failed to mmap stream generator registers");
        goto cleanup;
    }
    stream_gen_vaddr = (AxiStreamGen_Regs_t*)((uint8_t*)stream_gen_map_base + (AXI_STREAM_GEN_BASE & (MAP_SIZE - 1)));

    printf("    - Writing 0x%X to TRANS_SIZE_REG\n", STREAM_BUFFER_SIZE);
    stream_gen_vaddr->TRANS_SIZE_REG = STREAM_BUFFER_SIZE;

    printf("    - Writing 1 to RESET_GENERATOR_REG (to de-assert reset)\n");
    stream_gen_vaddr->RESET_GENERATOR_REG = 1;

    printf("    - Writing 1 to START_REG\n");
    stream_gen_vaddr->START_REG = 1;

    // --- Main Test Loop ---
    dma_regs->INTR_0_MASK_REG = FDMA_IRQ_MASK_ALL;
    int current_desc_idx = 0;
    int transfers_completed = 0;
    int all_passed = 1;

    printf("\n*****************************************************************\n");
    printf("  System configured. Waiting for %d transfers of %ld KB each.\n", NUM_STREAM_TRANSFERS, STREAM_BUFFER_SIZE / 1024);
    printf("*****************************************************************\n\n");

    dma_regs->STREAM_ADDR_REG[0] = STREAM_DESCRIPTOR_BASE;
    
    // --- NEW: Print debug state before waiting ---
    print_debug_state(dma_regs, stream_gen_vaddr, &desc_region_vaddr[0]);
    
    while(transfers_completed < NUM_STREAM_TRANSFERS) {
        printf("\nWaiting for interrupt for transfer %d (buffer %d)...\n", transfers_completed, current_desc_idx);
        
        uint32_t irq_count;
        ssize_t ret = read(dma_uio_fd, &irq_count, sizeof(irq_count));
        if (ret != sizeof(irq_count)) {
            perror("Interrupt read failed");
            break;
        }

        printf("  Interrupt received for Buffer %d.\n", current_desc_idx);
        dma_regs->INTR_0_CLEAR_REG = FDMA_IRQ_CLEAR_ALL;

        int next_desc_idx = (current_desc_idx + 1) % NUM_STREAM_BUFFERS;
        dma_regs->STREAM_ADDR_REG[0] = STREAM_DESCRIPTOR_BASE + (next_desc_idx * sizeof(DmaStreamDescriptor_t));
        
        if (!verify_stream_data(dest_bufs[current_desc_idx], STREAM_BUFFER_SIZE, transfers_completed)) {
            all_passed = 0;
        }
        
        (desc_region_vaddr + current_desc_idx)->CONFIG_REG |= STREAM_FLAG_DEST_RDY;
        
        uint32_t irq_enable = 1;
        write(dma_uio_fd, &irq_enable, sizeof(irq_enable));
        
        transfers_completed++;
        current_desc_idx = next_desc_idx;
    }

    dma_regs->INTR_0_MASK_REG = 0;
    printf("\nAll %d transfers complete.\n", transfers_completed);

    if(all_passed) {
        printf("\n***** AXI-Stream Ping-Pong Test PASSED *****\n");
    } else {
        printf("\n***** AXI-Stream Ping-Pong Test FAILED *****\n");
    }

cleanup:
    if (stream_gen_vaddr) {
        stream_gen_vaddr->START_REG = 0;
        stream_gen_vaddr->RESET_GENERATOR_REG = 0;
    }
    if (stream_gen_map_base) munmap(stream_gen_map_base, MAP_SIZE);
    if (desc_region_vaddr) munmap(desc_region_vaddr, actual_desc_size);
    for (int i = 0; i < NUM_STREAM_BUFFERS; ++i) {
        if(dest_bufs[i] && dest_bufs[i] != MAP_FAILED) munmap(dest_bufs[i], STREAM_BUFFER_SIZE);
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

// --- Helper Function (definitions) ---

void print_debug_state(CoreAXI4DMAController_Regs_t* dma_regs, AxiStreamGen_Regs_t* stream_gen, DmaStreamDescriptor_t* desc) {
    printf("\n--- PRE-WAIT HARDWARE STATE DUMP ---\n");

    // 1. Check Stream Generator State
    printf("[Stream Generator @ 0x%lX]\n", AXI_STREAM_GEN_BASE);
    printf("  - Read back TRANS_SIZE_REG   (0x00): 0x%08X\n", stream_gen->TRANS_SIZE_REG);
    printf("  - Read back START_REG        (0x04): 0x%08X\n", stream_gen->START_REG);
    printf("  - Read back RESET_GENERATOR_REG(0x08): 0x%08X\n", stream_gen->RESET_GENERATOR_REG);

    // 2. Check DMA Controller State
    printf("[DMA Controller @ 0x%lX]\n", (uintptr_t)dma_regs);
    printf("  - INTR_0_MASK_REG (0x14): 0x%08X\n", dma_regs->INTR_0_MASK_REG);
    printf("  - STREAM_ADDR_REG[0](0x460): 0x%08X\n", dma_regs->STREAM_ADDR_REG[0]);
    printf("  - INTR_0_STAT_REG (0x10): 0x%08X\n", dma_regs->INTR_0_STAT_REG);

    // 3. Check First Descriptor in Memory
    printf("[First Descriptor in DDR @ 0x%lX]\n", STREAM_DESCRIPTOR_BASE);
    printf("  - CONFIG_REG      (0x00): 0x%08X\n", desc->CONFIG_REG);
    printf("  - BYTE_COUNT_REG  (0x04): 0x%08X\n", desc->BYTE_COUNT_REG);
    printf("  - DEST_ADDR_REG   (0x08): 0x%08X\n", desc->DEST_ADDR_REG);
    
    printf("------------------------------------\n");
}

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
    dma_regs->INTR_0_MASK_REG = 0;
    dma_regs->INTR_0_CLEAR_REG = FDMA_IRQ_CLEAR_ALL;
    uint32_t dummy_irq_count;
    int flags = fcntl(dma_uio_fd, F_GETFL, 0);
    fcntl(dma_uio_fd, F_SETFL, flags | O_NONBLOCK);
    read(dma_uio_fd, &dummy_irq_count, sizeof(dummy_irq_count));
    fcntl(dma_uio_fd, F_SETFL, flags);
    uint32_t irq_enable = 1;
    write(dma_uio_fd, &irq_enable, sizeof(irq_enable));
}

int verify_stream_data(uint8_t* actual, size_t size, int transfer_num) {
    printf("\n--- Verifying transfer %d ---\n", transfer_num);
    uint8_t* expected = malloc(size);
    if (!expected) {
        printf("  ERROR: Failed to allocate memory for verification buffer.\n");
        return 0;
    }
    // This is a placeholder for generating expected data.
    memset(expected, (uint8_t)transfer_num, size);
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
    printf("  First 8 bytes received: ");
    for(int k=0; k<8; ++k) printf("%02X ", actual[k]);
    printf("\n");
    if (errors > 0) {
        printf("  ERROR: First mismatch at offset 0x%zX! Expected: 0x%02X, Got: 0x%02X\n", 
               first_error_offset, expected[first_error_offset], actual[first_error_offset]);
        free(expected);
        return 0;
    } else {
        printf("  SUCCESS: Data integrity verified for this transfer.\n");
    }
    free(expected);
    return 1;
}

#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include "dma_driver.h"
#include "hw_platform.h"

#define MAP_SIZE 4096UL // 4KB
#define MAP_MASK (MAP_SIZE - 1) // Mask for the mapping size

// Physical addresses for the loopback test buffers
#define TEST_SRC_BUFFER_ADDR (DDR_NON_CACHED_BASE_ADDR + 0x01000000) // 16MB offset
#define TEST_DEST_BUFFER_ADDR (DDR_NON_CACHED_BASE_ADDR + 0x01100000) // 17MB offset
#define TEST_BUFFER_SIZE 4096 // 4KB buffer size for the loopback test

static CoreAXI4DMAController_Regs_t* dma_regs = NULL;
static int mem_fd = -1;

int DMA_MapRegisters(void) {
    // Check if already mapped
    if (mem_fd != -1) return 1;
    // Open /dev/mem to access physical memory
    mem_fd = open("/dev/mem", O_RDWR | O_SYNC);
    // Check if the file descriptor is valid
    if (mem_fd < 0) { perror("Failed to open /dev/mem"); return 0; }
    // Map the DMA controller registers into the process's address space using mmap
    void* map_base = mmap(0, MAP_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, mem_fd, DMA_CONTROLLER_0_BASE_ADDR & ~MAP_MASK);
    // Check if the mapping was successful
    if (map_base == MAP_FAILED) { perror("mmap failed"); close(mem_fd); mem_fd = -1; return 0; }
    dma_regs = (CoreAXI4DMAController_Regs_t*)((uint8_t*)map_base + (DMA_CONTROLLER_0_BASE_ADDR & MAP_MASK));
    return 1;
}

void DMA_UnmapRegisters(void) {
    // Unmap the DMA controller registers and close the file descriptor
    if (dma_regs != NULL) { munmap((void*)((uintptr_t)dma_regs & ~MAP_MASK), MAP_SIZE); dma_regs = NULL; }
    if (mem_fd != -1) { close(mem_fd); mem_fd = -1; }
}


int DMA_GetInterruptStatus(void) {
    // Check if DMA registers are mapped and return the interrupt status for Descriptor 0
    if (dma_regs && (dma_regs->INTR_0_STAT_REG & 0x1)) { return (dma_regs->INTR_0_STAT_REG >> 4) & 0x3F; }
    return -1;
}

void DMA_ClearInterrupt(void) {
    // Check if DMA registers are mapped and clear the interrupt for Descriptor 0
    if (dma_regs) dma_regs->INTR_0_CLEAR_REG = 0x1;
}

// --- Diagnostic Loopback Test ---
int DMA_RunMemoryLoopbackTest(void) {
    // Ensure DMA registers are mapped
    if (dma_regs == NULL) return 0;

    printf("\n--- Starting DMA Memory-to-Memory Loopback Test ---\n");

    // Map source and destination buffers
    void* src_map_base = mmap(0, TEST_BUFFER_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, mem_fd, TEST_SRC_BUFFER_ADDR & ~MAP_MASK);
    void* dest_map_base = mmap(0, TEST_BUFFER_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, mem_fd, TEST_DEST_BUFFER_ADDR & ~MAP_MASK);

    // Check if the mappings were successful
    if (src_map_base == MAP_FAILED || dest_map_base == MAP_FAILED) {
        perror("Failed to map test buffers");
        if(src_map_base != MAP_FAILED) munmap(src_map_base, TEST_BUFFER_SIZE);
        if(dest_map_base != MAP_FAILED) munmap(dest_map_base, TEST_BUFFER_SIZE);
        return 0;
    }
    uint8_t* src_buf = (uint8_t*)((uint8_t*)src_map_base + (TEST_SRC_BUFFER_ADDR & MAP_MASK));
    uint8_t* dest_buf = (uint8_t*)((uint8_t*)dest_map_base + (TEST_DEST_BUFFER_ADDR & MAP_MASK));
    
    // Prepare buffers: Fill source with a pattern, clear destination
    printf("  - Preparing buffers...\n");
    for(int i = 0; i < TEST_BUFFER_SIZE; ++i) { src_buf[i] = (uint8_t)(i % 256); }
    memset(dest_buf, 0, TEST_BUFFER_SIZE);

    // Configure Internal Descriptor 0 for the mem-to-mem copy
    printf("  - Configuring Internal Descriptor 0...\n");
    DmaDescriptor_t* desc = &dma_regs->DESCRIPTOR[0];
    desc->SOURCE_ADDR_REG = (uint32_t)TEST_SRC_BUFFER_ADDR;
    desc->DEST_ADDR_REG = (uint32_t)TEST_DEST_BUFFER_ADDR;
    desc->BYTE_COUNT_REG = TEST_BUFFER_SIZE;
    desc->NEXT_DESC_ADDR_REG = 0; // Not chaining
    
    // According to datasheet, firmware must set VALID and both READY bits.
    uint32_t config = DESC_CONFIG_SOURCE_OP_INCR | DESC_CONFIG_DEST_OP_INCR |
                      DESC_CONFIG_INTR_ON_PROCESS | DESC_CONFIG_SOURCE_DATA_VALID |
                      DESC_CONFIG_DEST_DATA_READY;
    desc->CONFIG_REG = config;
    desc->CONFIG_REG |= DESC_CONFIG_DESCRIPTOR_VALID; // Set VALID last

    // Start the transfer by writing to the start register
    printf("  - Kicking off transfer for Descriptor 0...\n");
    dma_regs->INTR_0_MASK_REG = 0x1; // Enable completion interrupt
    dma_regs->START_OPERATION_REG = (1U << 0);

    // Wait for completion interrupt
    printf("  - Waiting for completion interrupt...");
    fflush(stdout);
    int success = 0;
    for (int i = 0; i < 5; ++i) { // 5 second timeout
        if (DMA_GetInterruptStatus() == 0) { // Expect completion for descriptor 0
            printf(" OK!\n");
            DMA_ClearInterrupt();
            success = 1;
            break;
        }
        sleep(1);
        printf(".");
        fflush(stdout);
    }
    if (!success) {
        printf(" TIMEOUT!\n");
    }

    // Verify data if transfer completed
    if (success) {
        printf("  - Verifying data...\n");
        if (memcmp(src_buf, dest_buf, TEST_BUFFER_SIZE) == 0) {
            printf("  - Data verification successful!\n");
        } else {
            printf("  - DATA MISMATCH! Loopback test failed.\n");
            success = 0;
        }
    }
    
    munmap(src_map_base, TEST_BUFFER_SIZE);
    munmap(dest_map_base, TEST_BUFFER_SIZE);
    
    return success;
}
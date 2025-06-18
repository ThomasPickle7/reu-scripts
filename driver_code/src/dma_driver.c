#include <stdio.h>
#include <inttypes.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include "dma_driver.h"
#include "hw_platform.h"

#define MAP_SIZE 4096UL
#define MAP_MASK (MAP_SIZE - 1)

// Pointer to the virtual memory address of the DMA controller
static CoreAXI4DMAController_Regs_t* dma_regs = NULL;
// File descriptor for /dev/mem
static int mem_fd = -1;

/**
 * Maps the physical DMA controller registers into the process's virtual address space.
 */
int DMA_MapRegisters(void) {
    if (mem_fd != -1) {
        printf("Warning: Registers already mapped.\n");
        return 1;
    }
    
    mem_fd = open("/dev/mem", O_RDWR | O_SYNC);
    if (mem_fd < 0) {
        perror("Failed to open /dev/mem");
        return 0;
    }
    
    void* map_base = mmap(0, MAP_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, mem_fd, DMA_CONTROLLER_0_BASE_ADDR & ~MAP_MASK);
    if (map_base == MAP_FAILED) {
        perror("mmap failed");
        close(mem_fd);
        mem_fd = -1;
        return 0;
    }
    
    dma_regs = (CoreAXI4DMAController_Regs_t*)((uint8_t*)map_base + (DMA_CONTROLLER_0_BASE_ADDR & MAP_MASK));
    return 1;
}

/**
 * Unmaps the DMA controller registers and closes the memory file descriptor.
 */
void DMA_UnmapRegisters(void) {
    if (dma_regs != NULL) {
        void* map_base = (void*)((uintptr_t)dma_regs & ~MAP_MASK);
        munmap(map_base, MAP_SIZE);
        dma_regs = NULL;
    }
    if (mem_fd != -1) {
        close(mem_fd);
        mem_fd = -1;
    }
}

/**
 * Configures the DMA for a single AXI-Stream to Memory transfer.
 */
int DMA_SetupStreamToMemory(uintptr_t descriptor_phys_addr, uintptr_t buffer_phys_addr, size_t buffer_size) {
    if (dma_regs == NULL) {
        fprintf(stderr, "Error: DMA registers not mapped.\n");
        return 0;
    }

    printf("--- Configuring Stream-to-Memory Transfer ---\n");

    // Map the physical memory for the stream descriptor into our virtual address space
    void* desc_map_base = mmap(0, 4096, PROT_READ | PROT_WRITE, MAP_SHARED, mem_fd, descriptor_phys_addr & ~MAP_MASK);
    if (desc_map_base == MAP_FAILED) {
        perror("Failed to map stream descriptor memory");
        return 0;
    }
    StreamDescriptor_t* stream_desc = (StreamDescriptor_t*)((uint8_t*)desc_map_base + (descriptor_phys_addr & MAP_MASK));

    // STEP 1: Populate the stream descriptor fields in system memory
    stream_desc->DEST_ADDR_REG = (uint32_t)buffer_phys_addr;
    stream_desc->BYTE_COUNT_REG = buffer_size & 0x007FFFFF; // Max size is ~8MB
    
    // STEP 2: Set the configuration. Must be marked VALID and READY.
    uint32_t config = STREAM_DESC_CONFIG_DEST_OP_INCR |  // Write to incrementing addresses
                      STREAM_DESC_CONFIG_DATA_READY |     // IMPORTANT: Tell DMA the buffer is ready
                      STREAM_DESC_CONFIG_VALID;           // IMPORTANT: Tell DMA the descriptor is valid
    stream_desc->CONFIG_REG = config;
    
    printf("  - Stream descriptor at P:0x%" PRIxPTR " configured to write %zu bytes to P:0x%" PRIxPTR "\n",
           descriptor_phys_addr, buffer_size, buffer_phys_addr);

    // STEP 3: Point the DMA controller to our stream descriptor in memory.
    // Your hardware connects to TDEST=0, so we MUST use STREAM_0_ADDR_REG.
    dma_regs->STREAM_0_ADDR_REG = (uint32_t)descriptor_phys_addr;
    printf("  - Wrote descriptor address to DMA's STREAM_0_ADDR_REG.\n");

    // Unmap the descriptor memory now that we've written to it
    munmap(desc_map_base, 4096);
    
    // STEP 4: Enable the "operation complete" interrupt
    dma_regs->INTR_0_MASK_REG = 0x1;

    printf("--- DMA is ARMED. Ready to receive stream data. ---\n");
    return 1;
}

/**
 * Checks for a completed operation by reading the interrupt status register.
 */
int DMA_GetCompletedDescriptor(void) {
    if (dma_regs && (dma_regs->INTR_0_STAT_REG & 0x1)) { // Check OPS_COMPL bit
        return (dma_regs->INTR_0_STAT_REG >> 4) & 0x3F; // Return descriptor number
    }
    return -1; // No completion interrupt
}

/**
 * Clears the "operation complete" flag in the interrupt clear register.
 */
void DMA_ClearCompletionInterrupt(void) {
    if (dma_regs) {
        dma_regs->INTR_0_CLEAR_REG = 0x1;
    }
}

#include <stdio.h>
#include <inttypes.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include "dma_driver.h"
#include "hw_platform.h"

#define MAP_SIZE 4096UL
#define MAP_MASK (MAP_SIZE - 1)

static CoreAXI4DMAController_Regs_t* dma_regs = NULL;
static int mem_fd = -1;

// --- Mapping/Unmapping Functions (Unchanged) ---
int DMA_MapRegisters(void) {
    mem_fd = open("/dev/mem", O_RDWR | O_SYNC);
    if (mem_fd < 0) { perror("Failed to open /dev/mem"); return 0; }
    void* map_base = mmap(0, MAP_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, mem_fd,
                          DMA_CONTROLLER_0_BASE_ADDR & ~MAP_MASK);
    if (map_base == MAP_FAILED) { perror("mmap failed"); close(mem_fd); return 0; }
    dma_regs = (CoreAXI4DMAController_Regs_t*)((uint8_t*)map_base + (DMA_CONTROLLER_0_BASE_ADDR & MAP_MASK));
    return 1;
}
void DMA_UnmapRegisters(void) {
    if (dma_regs) {
        void* map_base = (void*)((uintptr_t)dma_regs & ~MAP_MASK);
        munmap(map_base, MAP_SIZE); dma_regs = NULL;
    }
    if (mem_fd != -1) { close(mem_fd); mem_fd = -1; }
}

// --- Driver Functions (Unchanged) ---
int DMA_InitCyclicStream(uint8_t num_descriptors, volatile void* buffers[], size_t buffer_size) {
    if (dma_regs == NULL) { fprintf(stderr, "Error: DMA registers not mapped.\n"); return 0; }
    if (num_descriptors == 0 || num_descriptors > 32 || buffers == NULL || buffer_size == 0) return 0;
    printf("--- Initializing DMA for %u-buffer cyclic stream transfer ---\n", num_descriptors);
    for (uint8_t i = 0; i < num_descriptors; ++i) {
        DmaDescriptor_t* desc = &dma_regs->DESCRIPTOR[i];
        uint8_t next_desc_num = (i + 1) % num_descriptors;
        desc->SOURCE_ADDR_REG = 0;
        desc->DEST_ADDR_REG = (uint32_t)(uintptr_t)buffers[i];
        desc->BYTE_COUNT_REG = buffer_size & 0x007FFFFF;
        desc->NEXT_DESC_ADDR_REG = next_desc_num;
        uint32_t config = (DESC_OPR_NO_OP << 0) | (DESC_OPR_INCREMENTING << 2) | DESC_CONFIG_CHAIN |
                          DESC_CONFIG_INTR_ON_PROCESS | DESC_CONFIG_SOURCE_DATA_VALID | DESC_CONFIG_DEST_DATA_READY;
        desc->CONFIG_REG = config;
        desc->CONFIG_REG |= DESC_CONFIG_DESCRIPTOR_VALID;
        printf("  - Descriptor %u: Configured for Buffer at 0x%" PRIxPTR "\n", i, (uintptr_t)buffers[i]);
    }
    dma_regs->INTR_0_MASK_REG = 0x1;
    printf("--- DMA Initialization Complete ---\n");
    return 1;
}

// --- NEW VERIFICATION FUNCTION ---
int DMA_VerifyConfig(uint8_t descriptor_num, volatile void* buffers[], size_t buffer_size) {
    if (dma_regs == NULL || descriptor_num >= 32) return 0;

    printf("--- Verifying configuration of Descriptor %u ---\n", descriptor_num);
    DmaDescriptor_t* desc = &dma_regs->DESCRIPTOR[descriptor_num];
    int success = 1;

    // Verify Destination Address
    uint32_t expected_dest = (uint32_t)(uintptr_t)buffers[descriptor_num];
    if (desc->DEST_ADDR_REG != expected_dest) {
        printf("  FAIL: DEST_ADDR. Expected: 0x%X, Read: 0x%X\n", expected_dest, desc->DEST_ADDR_REG);
        success = 0;
    } else {
        printf("  OK: DEST_ADDR verified.\n");
    }

    // Verify Byte Count
    uint32_t expected_bytes = buffer_size & 0x007FFFFF;
    if (desc->BYTE_COUNT_REG != expected_bytes) {
        printf("  FAIL: BYTE_COUNT. Expected: %u, Read: %u\n", expected_bytes, desc->BYTE_COUNT_REG);
        success = 0;
    } else {
        printf("  OK: BYTE_COUNT verified.\n");
    }
    
    if (success) printf("--- Verification PASSED. CPU can talk to DMA correctly. ---\n");
    else printf("--- Verification FAILED. Check address map or bus connections. ---\n");

    return success;
}


// --- Other Driver Functions (Unchanged) ---
void DMA_StartCyclic(uint8_t d_num) { if (dma_regs) dma_regs->START_OPERATION_REG = (1U << d_num); }
int DMA_GetCompletedDescriptor(void) { if (dma_regs && (dma_regs->INTR_0_STAT_REG & 1)) return (dma_regs->INTR_0_STAT_REG >> 4) & 0x3F; return -1; }
void DMA_ClearCompletionInterrupt(void) { if (dma_regs) dma_regs->INTR_0_CLEAR_REG = 0x1; }
void DMA_ReturnBuffer(uint8_t d_num) { if (dma_regs && d_num < 32) dma_regs->DESCRIPTOR[d_num].CONFIG_REG |= DESC_CONFIG_DEST_DATA_READY; }
void DMA_DebugDumpRegisters(uint8_t d_num) { /* ... implementation from before ... */ }

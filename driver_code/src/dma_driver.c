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

// Maps the DMA controller's registers.
int DMA_MapRegisters(void) {
    if (mem_fd != -1) return 1;
    mem_fd = open("/dev/mem", O_RDWR | O_SYNC);
    if (mem_fd < 0) { perror("Failed to open /dev/mem"); return 0; }
    void* map_base = mmap(0, MAP_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, mem_fd, DMA_CONTROLLER_0_BASE_ADDR & ~MAP_MASK);
    if (map_base == MAP_FAILED) { perror("mmap failed"); close(mem_fd); mem_fd = -1; return 0; }
    dma_regs = (CoreAXI4DMAController_Regs_t*)((uint8_t*)map_base + (DMA_CONTROLLER_0_BASE_ADDR & MAP_MASK));
    return 1;
}

// Unmaps the DMA controller's registers.
void DMA_UnmapRegisters(void) {
    if (dma_regs != NULL) { munmap((void*)((uintptr_t)dma_regs & ~MAP_MASK), MAP_SIZE); dma_regs = NULL; }
    if (mem_fd != -1) { close(mem_fd); mem_fd = -1; }
}

// STEP 1 of the handshake: Prepare the descriptor and point the DMA to it.
int DMA_ArmStream(uintptr_t descriptor_phys_addr, uintptr_t buffer_phys_addr, size_t buffer_size) {
    if (dma_regs == NULL) { fprintf(stderr, "Error: DMA registers not mapped.\n"); return 0; }

    void* desc_map_base = mmap(0, 4096, PROT_READ | PROT_WRITE, MAP_SHARED, mem_fd, descriptor_phys_addr & ~MAP_MASK);
    if (desc_map_base == MAP_FAILED) { perror("Failed to map stream descriptor memory"); return 0; }
    StreamDescriptor_t* stream_desc = (StreamDescriptor_t*)((uint8_t*)desc_map_base + (descriptor_phys_addr & MAP_MASK));

    // Populate the descriptor but leave DATA_READY bit CLEAR.
    stream_desc->DEST_ADDR_REG = (uint32_t)buffer_phys_addr;
    stream_desc->BYTE_COUNT_REG = buffer_size;
    stream_desc->CONFIG_REG = STREAM_DESC_CONFIG_DEST_OP_INCR | STREAM_DESC_CONFIG_VALID;

    // Point the DMA to this descriptor.
    dma_regs->STREAM_0_ADDR_REG = (uint32_t)descriptor_phys_addr;

    // Enable the completion interrupt.
    dma_regs->INTR_0_MASK_REG = 0x1;

    munmap(desc_map_base, 4096);
    return 1;
}

// STEP 2 of the handshake: Set the DATA_READY bit after getting the first interrupt.
void DMA_ProvideBuffer(uintptr_t descriptor_phys_addr) {
    if (dma_regs == NULL) { fprintf(stderr, "Error: DMA registers not mapped.\n"); return; }

    void* desc_map_base = mmap(0, 4096, PROT_READ | PROT_WRITE, MAP_SHARED, mem_fd, descriptor_phys_addr & ~MAP_MASK);
    if (desc_map_base == MAP_FAILED) { perror("Failed to map stream descriptor memory for update"); return; }
    StreamDescriptor_t* stream_desc = (StreamDescriptor_t*)((uint8_t*)desc_map_base + (descriptor_phys_addr & MAP_MASK));
    
    // Set the data ready bit to un-stall the DMA.
    stream_desc->CONFIG_REG |= STREAM_DESC_CONFIG_DATA_READY;

    munmap(desc_map_base, 4096);
}


int DMA_GetInterruptStatus(void) {
    if (dma_regs && (dma_regs->INTR_0_STAT_REG & 0x1)) {
        return (dma_regs->INTR_0_STAT_REG >> 4) & 0x3F;
    }
    return -1;
}

void DMA_ClearInterrupt(void) {
    if (dma_regs) dma_regs->INTR_0_CLEAR_REG = 0x1;
}

void DMA_PrintDataBuffer(uintptr_t buffer_phys_addr, size_t bytes_to_print) {
    if (mem_fd < 0) { fprintf(stderr, "Error: /dev/mem not open.\n"); return; }
    if (bytes_to_print == 0) return;
    
    void* buffer_map_base = mmap(0, 4096, PROT_READ, MAP_SHARED, mem_fd, buffer_phys_addr & ~MAP_MASK);
    if (buffer_map_base == MAP_FAILED) { perror("Failed to map data buffer for printing"); return; }
    volatile uint8_t* data = (volatile uint8_t*)((uint8_t*)buffer_map_base + (buffer_phys_addr & MAP_MASK));
    
    printf("Data Buffer at P:0x%" PRIxPTR " contains:\n  [", buffer_phys_addr);
    for(size_t i = 0; i < bytes_to_print && i < 4096; ++i) {
        printf("0x%02X%s", data[i], (i == bytes_to_print - 1 || i == 15) ? "" : ", ");
        if (i == 15 && bytes_to_print > 16) {
            printf(" ...");
            break;
        }
    }
    printf("]\n");
    munmap(buffer_map_base, 4096);
}

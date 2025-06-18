#include <stdio.h>
#include <inttypes.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include "dma_driver.h"
#include "hw_platform.h"

// Define the size of the data buffer
#define BUFFER_SIZE_BYTES 4096

// Reserve a fixed physical address for our Stream Descriptor.
// This must be in a memory region accessible by both the CPU and the DMA.
#define STREAM_DESCRIPTOR_PHYS_ADDR (DDR_NON_CACHED_BASE_ADDR + 0x00100000) // 1MB offset

// Reserve a fixed physical address for our data buffer.
#define DATA_BUFFER_PHYS_ADDR (DDR_NON_CACHED_BASE_ADDR + 0x00200000) // 2MB offset

int main() {
    printf("--- AXI-Stream to Memory DMA Application ---\n");
    printf("--- Current Time: %s\n", __TIME__);
    printf("--- Current Date: %s\n", __DATE__);

    if (!DMA_MapRegisters()) {
        fprintf(stderr, "Fatal: Could not map DMA registers. Are you running with sudo?\n");
        exit(1);
    }

    // Initialize the DMA for a stream-to-memory transfer.
    if (!DMA_SetupStreamToMemory(STREAM_DESCRIPTOR_PHYS_ADDR, DATA_BUFFER_PHYS_ADDR, BUFFER_SIZE_BYTES)) {
        fprintf(stderr, "Fatal: DMA Stream Initialization Failed.\n");
        DMA_UnmapRegisters();
        exit(1);
    }

    // There is NO start command. The transfer is initiated by the FPGA stream source.
    // We just wait for the completion interrupt.

    printf("\nWaiting for DMA completion interrupt (Timeout: 5 seconds)...\n");

    const int timeout_seconds = 5;
    for (int i = 0; i < timeout_seconds; i++) {
        // According to the datasheet, a completed stream transfer is reported
        // with descriptor number 33.
        if (DMA_GetCompletedDescriptor() == 33) {
            printf("\nSUCCESS! Stream operation complete interrupt received.\n");

            // To verify the data, we must map the physical buffer address.
            void* buffer_map_base = mmap(0, 4096, PROT_READ, MAP_SHARED, fileno(stdin), DATA_BUFFER_PHYS_ADDR & ~0xFFF);
            if(buffer_map_base != MAP_FAILED) {
                 volatile uint8_t* received_data = (volatile uint8_t*)((uint8_t*)buffer_map_base + (DATA_BUFFER_PHYS_ADDR & 0xFFF));
                 printf("  - Data verification: First 4 bytes are [0x%02X, 0x%02X, 0x%02X, 0x%02X]\n",
                   received_data[0], received_data[1], received_data[2], received_data[3]);
                 munmap(buffer_map_base, 4096);
            } else {
                perror("Could not map data buffer for verification");
            }

            DMA_ClearCompletionInterrupt();
            DMA_UnmapRegisters();
            return 0;
        }
        sleep(1);
        printf(".");
        fflush(stdout);
    }

    printf("\n\n!!! TIMEOUT: No stream completion interrupt was received.\n");
    printf("Please check:\n");
    printf("  1. The FPGA is programmed and the stream source is running.\n");
    printf("  2. The physical memory addresses in hw_platform.h are correct.\n");
    printf("  3. The interrupt connection from the FPGA to the CPU is correct.\n");

    DMA_UnmapRegisters();
    return 1;
}

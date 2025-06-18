#include <stdio.h>
#include <inttypes.h>
#include <stdlib.h>
#include <unistd.h>
#include "dma_driver.h"
#include "hw_platform.h"

#define NUM_DMA_BUFFERS 4
#define BUFFER_SIZE_BYTES 4096

volatile uint8_t dma_buffer_0[BUFFER_SIZE_BYTES];
volatile uint8_t dma_buffer_1[BUFFER_SIZE_BYTES];
volatile uint8_t dma_buffer_2[BUFFER_SIZE_BYTES];
volatile uint8_t dma_buffer_3[BUFFER_SIZE_BYTES];

volatile void* dma_buffers[NUM_DMA_BUFFERS] = {
    dma_buffer_0, dma_buffer_1, dma_buffer_2, dma_buffer_3
};

int main() {
    printf("--- DMA Stream Capture Application ---\n");

    if (!DMA_MapRegisters()) {
        fprintf(stderr, "Fatal: Could not map DMA registers. Are you running with sudo?\n");
        exit(1);
    }

    if (!DMA_InitCyclicStream(NUM_DMA_BUFFERS, dma_buffers, BUFFER_SIZE_BYTES)) {
        printf("Fatal: DMA Initialization Failed. Halting.\n");
        DMA_UnmapRegisters();
        exit(1);
    }

    // --- NEW VERIFICATION STEP ---
    // After configuring the DMA, we read back the values to ensure they were written correctly.
    // We only need to check one descriptor to confirm the control bus is working.
    if (!DMA_VerifyConfig(0, dma_buffers, BUFFER_SIZE_BYTES)) {
         printf("Fatal: DMA configuration could not be verified. Halting.\n");
         DMA_UnmapRegisters();
         exit(1);
    }
    // If we get here, we know the CPU can successfully write to and read from the DMA registers.

    DMA_StartCyclic(0);

    const int timeout_limit = 5000000;
    int timeout_counter = 0;

    while(1) {
        int completed_desc = DMA_GetCompletedDescriptor();

        if (completed_desc != -1) {
            timeout_counter = 0; // Reset timeout on success

            printf("CPU: Interrupt! Descriptor %d (Buffer at 0x%" PRIxPTR ") is full.\n",
                   completed_desc, (uintptr_t)dma_buffers[completed_desc]);

            volatile uint8_t* received_data = (volatile uint8_t*)dma_buffers[completed_desc];
            printf("  - Processing data... First byte is 0x%02X.\n", received_data[0]);

            DMA_ClearCompletionInterrupt();
            DMA_ReturnBuffer(completed_desc);

        } else {
            timeout_counter++;
            if (timeout_counter >= timeout_limit) {
                printf("\n!!! TIMEOUT: No DMA completion interrupt received.\n");
                DMA_DebugDumpRegisters(0);
                timeout_counter = 0;
            }
        }
        usleep(1);
    }
    
    DMA_UnmapRegisters();
    return 0;
}

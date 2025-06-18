#include <stdio.h>
#include <inttypes.h>
#include <stdlib.h>
#include <unistd.h>
#include "dma_driver.h"
#include "hw_platform.h"

// Physical address for our Stream Descriptor in non-cached memory
#define STREAM_DESCRIPTOR_PHYS_ADDR (DDR_NON_CACHED_BASE_ADDR + 0x00100000)

// Physical address for our data buffer in non-cached memory
#define DATA_BUFFER_PHYS_ADDR (DDR_NON_CACHED_BASE_ADDR + 0x00200000)
#define BUFFER_SIZE_BYTES 4096

typedef enum {
    STATE_START,
    STATE_ARM_DMA,
    STATE_WAIT_FOR_BUFFER_REQUEST,
    STATE_PROVIDE_BUFFER,
    STATE_WAIT_FOR_COMPLETION,
    STATE_DONE,
    STATE_TIMEOUT,
    STATE_ERROR
} AppState_t;

int main() {
    printf("--- DMA Stream Capture Application (Corrected Handshake Logic) ---\n");

    if (!DMA_MapRegisters()) {
        fprintf(stderr, "Fatal: Could not map DMA registers.\n");
        exit(1);
    }

    AppState_t state = STATE_START;
    int timeout_counter = 10; // 10 second timeout

    while (state != STATE_DONE && state != STATE_TIMEOUT && state != STATE_ERROR) {
        switch (state) {
            case STATE_START:
                printf("State: START -> ARM_DMA\n");
                state = STATE_ARM_DMA;
                break;

            case STATE_ARM_DMA:
                if (DMA_ArmStream(STREAM_DESCRIPTOR_PHYS_ADDR, DATA_BUFFER_PHYS_ADDR, BUFFER_SIZE_BYTES)) {
                    printf("State: ARM_DMA -> WAIT_FOR_BUFFER_REQUEST\n");
                    printf("DMA is armed. Waiting for FPGA to start streaming...\n");
                    state = STATE_WAIT_FOR_BUFFER_REQUEST;
                } else {
                    fprintf(stderr, "Error: Failed to arm DMA.\n");
                    state = STATE_ERROR;
                }
                break;

            case STATE_WAIT_FOR_BUFFER_REQUEST:
                if (DMA_GetInterruptStatus() == 33) {
                    printf("\nState: WAIT_FOR_BUFFER_REQUEST -> PROVIDE_BUFFER\n");
                    printf("Interrupt received! DMA is requesting a buffer.\n");
                    DMA_ClearInterrupt();
                    state = STATE_PROVIDE_BUFFER;
                }
                break;

            case STATE_PROVIDE_BUFFER:
                DMA_ProvideBuffer(STREAM_DESCRIPTOR_PHYS_ADDR);
                printf("State: PROVIDE_BUFFER -> WAIT_FOR_COMPLETION\n");
                printf("Buffer provided to DMA. Waiting for final transfer completion...\n");
                state = STATE_WAIT_FOR_COMPLETION;
                break;

            case STATE_WAIT_FOR_COMPLETION:
                 if (DMA_GetInterruptStatus() == 33) {
                    printf("\nState: WAIT_FOR_COMPLETION -> DONE\n");
                    printf("SUCCESS: Final DMA completion interrupt received!\n");
                    DMA_ClearInterrupt();
                    state = STATE_DONE;
                }
                break;
                
            default:
                state = STATE_ERROR;
                break;
        }

        sleep(1);
        if (state == STATE_WAIT_FOR_BUFFER_REQUEST || state == STATE_WAIT_FOR_COMPLETION) {
            timeout_counter--;
            printf(".");
            fflush(stdout);
            if (timeout_counter <= 0) {
                state = STATE_TIMEOUT;
            }
        }
    }

    if (state == STATE_DONE) {
        printf("\n--- Verification ---\n");
        DMA_PrintDataBuffer(DATA_BUFFER_PHYS_ADDR, 16);
    } else if (state == STATE_TIMEOUT) {
        printf("\n\n!!! TIMEOUT occurred.\n");
    } else {
        printf("\n\nAn unexpected error occurred.\n");
    }

    DMA_UnmapRegisters();
    return (state == STATE_DONE) ? 0 : 1;
}

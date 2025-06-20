#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "dma_driver.h"
#include "mpu_driver.h" // Include the new MPU driver header

int main() {
    printf("--- DMA Sanity Check Application ---\n");

    // Configure the MPU to grant fabric access to DDR memory.
    // This is required, otherwise the DMA will be blocked by default.
    if (!MPU_Configure_FIC0()) {
        fprintf(stderr, "Fatal: Could not configure MPU. Halting.\n");
        exit(1);
    }

    //  Map the DMA controller registers.
    if (!DMA_MapRegisters()) {
        fprintf(stderr, "Fatal: Could not map DMA registers. Are you running with sudo?\n");
        exit(1);
    }

    // Run the self-contained loopback test.
    int test_passed = DMA_RunMemoryLoopbackTest();

    if (test_passed) {
        printf("\n============================================\n");
        printf("  LOOPBACK TEST PASSED\n");
        printf("============================================\n");
        printf("This confirms the DMA can be configured and can access memory correctly.\n");
        printf("You can now proceed to test the stream-to-memory functionality.\n");
    } else {
        printf("\n============================================\n");
        printf("  LOOPBACK TEST FAILED\n");
        printf("============================================\n");
        printf("Failure after MPU configuration suggests a deeper issue.\n");
        printf("Please re-verify system clocks, resets, and AXI interconnect routing.\n");
    }

    DMA_UnmapRegisters();
    return test_passed ? 0 : 1;
}

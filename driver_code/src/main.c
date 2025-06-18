#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "dma_driver.h"
#include "hw_platform.h"

int main() {
    printf("--- DMA Sanity Check Application ---\n");

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
        printf("The issue is likely isolated to the AXI-Stream source or stream-specific logic.\n");
    } else {
        printf("\n============================================\n");
        printf("  LOOPBACK TEST FAILED\n");
        printf("============================================\n");
        printf("This points to a fundamental issue:\n");
        printf(" - Check the AXI interconnect address map.\n");
        printf(" - Ensure the DMA's master port has access to DDR memory at 0x%lX.\n", (unsigned long)DDR_NON_CACHED_BASE_ADDR);
        printf(" - Verify system clocks and resets.\n");
    }

    DMA_UnmapRegisters();
    return test_passed ? 0 : 1;
}

/**************************************************************************************************
 * @file main.c
 * @author Thomas Pickle
 * @brief Main application file to run DMA tests.
 * @version 0.2
 * @date 2024-07-10
 *
 * @copyright Copyright (c) 2024
 *
 *************************************************************************************************/

#include "bsp.h"
#include "radiohound_dma_api.h"
#include <stdio.h>

#define NUM_TRANSFERS 16 // Total number of buffers to transfer

int main() {
    // Initialize the board hardware (MPU, PLIC, etc.)
    bsp_init();

    // Initialize the RadioHound DMA API (which initializes the low-level driver)
    rh_dma_init();

    printf("======== DMA Test Application Started ========\n");

    // Run the main DMA test
    int result = rh_dma_run_m2m_ping_pong_test(NUM_TRANSFERS);

    // Report final status
    if (result == 0) {
        printf("\n***** Mem-to-Mem Ping-Pong Test PASSED *****\n");
    } else {
        printf("\n***** Mem-to-Mem Ping-Pong Test FAILED *****\n");
    }

    printf("======== DMA Test Application Finished ========\n");

    // Loop forever
    while(1);

    return 0;
}
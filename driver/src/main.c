/*******************************************************************************
 *******************************************************************************
 *
 * FILE: main.c
 *
 * DESCRIPTION:
 * Main application code running on U54_1. Uses the AXI DMA driver.
 *
 *******************************************************************************
 *******************************************************************************/
#include <stdio.h>
#include <string.h>
#include "mpfs_hal/mss_hal.h"
#include "drivers/mss/mss_mmuart/mss_uart.h"
#include "axi_dma_driver.h" // Include the new driver header

// This global is defined in the driver, but we can declare it here to use it.
extern volatile uint32_t g_dma_completed_buffer_flag;

void u54_1(void)
{
    uint8_t info_string[100];

    (void)mss_config_clk_rst(MSS_PERIPH_MMUART1, (uint8_t) 1, PERIPHERAL_ON);
    (void)mss_config_clk_rst(MSS_PERIPH_CFM, (uint8_t) 1, PERIPHERAL_ON);

    MSS_UART_init(&g_mss_uart1_lo,
                  MSS_UART_115200_BAUD,
                  MSS_UART_DATA_8_BITS | MSS_UART_NO_PARITY | MSS_UART_ONE_STOP_BIT);

    MSS_UART_polled_tx(&g_mss_uart1_lo, (const uint8_t*)"AXI DMA Continuous Stream Test\r\n", sizeof("AXI DMA Continuous Stream Test\r\n"));

    // Initialize the DMA driver
    dma_init();

    // Configure DMA controller 0 for continuous streaming
    dma_configure_continuous_stream(0);

    MSS_UART_polled_tx(&g_mss_uart1_lo, (const uint8_t*)"Starting continuous DMA transfer...\r\n", sizeof("Starting continuous DMA transfer...\r\n"));

    // Start the transfer on Descriptor 0. It will automatically loop.
    dma_start_transfer(0, 0);

    while (1)
    {
        // Wait for an interrupt, which signals a buffer is full
        if (g_dma_completed_buffer_flag)
        {
            g_dma_completed_buffer_flag = 0; // Clear the flag

            // This is where you would process the data in the completed buffer.
            // You would need to check which buffer (A or B) is ready by reading
            // the DMA status registers.
            sprintf((char*)info_string, "DMA Interrupt: A buffer is full and ready for processing.\r\n");
            MSS_UART_polled_tx(&g_mss_uart1_lo, info_string, strlen((char*)info_string));

            // IMPORTANT: In a real system, after processing the buffer, you must
            // re-arm the corresponding descriptor by setting its
            // DEST_DATA_READY bit again so the DMA can reuse it.
        }
    }
}

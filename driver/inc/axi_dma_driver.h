/*******************************************************************************
 *******************************************************************************
 *
 * FILE: axi_dma_driver.h
 *
 * DESCRIPTION:
 * Header file for the CoreAXI4DMAController driver. This defines the public
 * API for initializing the DMA, configuring transfers, and handling interrupts.
 *
 *******************************************************************************
 *******************************************************************************/
#ifndef AXI_DMA_DRIVER_H_
#define AXI_DMA_DRIVER_H_

#include <stdint.h>

/*------------------------------------------------------------------------------
 * Public Defines
 */

// Base addresses for the DMA controllers in the system
#define DMA0_BASE_ADDR                  0x60010000u
#define DMA1_BASE_ADDR                  0x60011000u
#define DMA2_BASE_ADDR                  0x60012000u
#define DMA3_BASE_ADDR                  0x60013000u

// Memory addresses for the ping-pong buffers used in continuous streaming
#define STREAM_BUFFER_A_ADDR            0xA0020000u
#define STREAM_BUFFER_B_ADDR            0xA0030000u
#define STREAM_CHUNK_SIZE               4096u // 4KB for each buffer

/*------------------------------------------------------------------------------
 * Public Function Declarations
 */

/**
 * @brief Initializes the DMA driver.
 * This function must be called once before any other DMA operations.
 */
void dma_init(void);

/**
 * @brief Configures a DMA controller for continuous AXI-Stream to memory
 * transfer using a circular chain of two descriptors (ping-pong).
 * @param dma_id The ID of the DMA controller to configure (0-3).
 */
void dma_configure_continuous_stream(uint8_t dma_id);

/**
 * @brief Starts a DMA transfer on a specific descriptor.
 * @param dma_id The ID of the DMA controller to use (0-3).
 * @param descriptor_id The descriptor to start the transfer on.
 */
void dma_start_transfer(uint8_t dma_id, uint8_t descriptor_id);

/**
 * @brief The public interrupt handler for the DMA.
 * This function should be registered with the PLIC.
 */
uint8_t fabric_f2h_6_plic_IRQHandler(void);

#endif /* AXI_DMA_DRIVER_H_ */



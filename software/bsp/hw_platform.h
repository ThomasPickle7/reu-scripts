/**************************************************************************************************
 * @file hw_platform.h
 * @author Thomas Pickle
 * @brief Hardware platform specific definitions
 * @version 0.2
 * @date 2024-07-10
 *
 * @copyright Copyright (c) 2024
 *
 *************************************************************************************************/
#ifndef HW_PLATFORM_H_
#define HW_PLATFORM_H_

#include <stdint.h>

// --- Memory Map ---
#define DDR_BASE_ADDR           0x80000000
#define DDR_SIZE                0x40000000  // 1GB
#define AXI_DMA_BASE_ADDR       0x60010000  // From UIO_DMA_DEVNAME
#define PLIC_BASE_ADDR          0x0C000000

// --- Interrupt Map ---
#define AXI_DMA_IRQ             10 // Example interrupt number

// --- PLIC Offsets ---
#define PLIC_EN_OFFSET          (PLIC_BASE_ADDR + 0x2000)

// --- DMA Buffer Configuration ---
#define NUM_BUFFERS             4
#define BUFFER_SIZE             (1024 * 1024) // 1MB

// Base address for all DMA buffers (assumes a contiguous block)
#define DMA_BUFFER_AREA_BASE    (DDR_BASE_ADDR + 0x10000000) // Place buffers at DDR + 256MB

// Offsets within the DMA buffer area
#define PING_PONG_SRC_OFFSET    0
#define PING_PONG_DEST_OFFSET   (PING_PONG_SRC_OFFSET + (NUM_BUFFERS * BUFFER_SIZE))

#endif /* HW_PLATFORM_H_ */

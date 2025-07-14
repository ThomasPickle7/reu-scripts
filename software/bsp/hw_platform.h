#ifndef HW_PLATFORM_H_
#define HW_PLATFORM_H_

#include <stdint.h>

// --- Base Addresses from Memory Map ---
#define FDMA_BASE_ADDR                 0x60010000UL
#define AXI_STREAM_SOURCE_BASE_ADDR    0x60000000UL
#define MPU_BASE_ADDR                  0x20005000UL

// --- Linux Device Identifiers ---
#define UIO_DMA_DEVNAME          "dma-controller@60010000"
#define UIO_STREAM_SRC_DEVNAME   "stream-source@60000000"
#define UDMA_BUF_DEVNAME         "/dev/udmabuf-ddr-nc0"
#define UDMA_BUF_SYNC_DEVNAME    "udmabuf-ddr-nc0" // Name used for getting phys addr

// --- DMA Buffer Layout Configuration ---
#define NUM_BUFFERS              4
#define BUFFER_SIZE              (1024 * 1024)

// These offsets are within the single large udmabuf
#define PING_PONG_SRC_OFFSET     0
#define PING_PONG_DEST_OFFSET    (PING_PONG_SRC_OFFSET + (NUM_BUFFERS * BUFFER_SIZE))
#define STREAM_DEST_OFFSET       (PING_PONG_DEST_OFFSET + (NUM_BUFFERS * BUFFER_SIZE))
#define STREAM_DESCRIPTOR_OFFSET (STREAM_DEST_OFFSET + (NUM_BUFFERS * BUFFER_SIZE))

#endif // HW_PLATFORM_H_

// =================================================================================================
// File: app_config.h
// Description: Contains high-level application configuration and constants.
// =================================================================================================

#ifndef APP_CONFIG_H
#define APP_CONFIG_H

#include <stddef.h>

// --- Physical Memory Layout ---
// NOTE: This physical base address is system-specific. For a more robust application,
// this should be read from sysfs at runtime rather than being hardcoded.
#define DMA_PHYSICAL_BASE_ADDR 0xc8000000

// --- Buffer Configuration within the UDMABuf region ---
#define STREAM_DESCRIPTOR_OFFSET 0x0      // Descriptors at the start of the buffer
#define STREAM_DEST_OFFSET       0x1000   // 4KB offset for the data destination buffer
#define DMA_BUFFER_SIZE          (STREAM_DEST_OFFSET + 8192) // Total size for descriptors + 8KB data buffer

#endif // APP_CONFIG_H


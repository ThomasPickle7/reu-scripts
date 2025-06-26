#ifndef HW_PLATFORM_H_
#define HW_PLATFORM_H_

// Base address for the CoreAXI4DMAController's AXI4-Lite control interface.
#define DMA_CONTROLLER_0_BASE_ADDR 0x60010000UL

// Base address for the MPU (Memory Protection Unit) configuration registers.
// MPUCFG is at 0x20005000. MPU1 for FIC0 is at offset 0.
#define MPU_BASE_ADDR              0x20005000UL

// Base address of the non-cached DDR memory region.
#define DDR_NON_CACHED_BASE_ADDR   0xC0000000UL

#endif

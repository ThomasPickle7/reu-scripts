/*
 * hw_platform.h
 *
 * This version is created based on the memory map you provided. It defines
 * the physical memory addresses of hardware peripherals for the C driver.
 */

#ifndef HW_PLATFORM_H_
#define HW_PLATFORM_H_

/**
 * @brief Base address for the CoreAXI4DMAController's AXI4-Lite control interface.
 *
 * Path: BVF_RISCV_SUBSYSTEM/Coreplex -> S1_FIC0_MSS_TO_FABRIC_AXI4_512MB ->
 * FPGA_MEM_INTERFACE_0/DMA_CONTROLLER_0:AXI4SlaveCtrl_IF
 * Value: 0x6001_0000
 */
#define DMA_CONTROLLER_0_BASE_ADDR 0x60010000UL

/**
 * @brief Base address of the non-cached DDR memory region.
 *
 * Using non-cached memory for DMA buffers can simplify software by removing
 * the need for manual cache invalidation, though CPU access may be slower.
 *
 * Path: BVF_RISCV_SUBSYSTEM/Coreplex -> S7_DDRC_NON_CACHED_256MB
 * Value: 0xC000_0000
 */
#define DDR_NON_CACHED_BASE_ADDR   0xC0000000UL


#endif /* HW_PLATFORM_H_ */


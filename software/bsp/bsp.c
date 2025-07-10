/**************************************************************************************************
 * @file bsp.c
 * @author Thomas Pickle
 * @brief Board Support Package
 * @version 0.1
 * @date 2024-07-09
 * * @copyright Copyright (c) 2024
 * *************************************************************************************************/

#include "bsp.h"
#include "mpu_driver.h"
#include "plic_driver.h"
#include "hw_platform.h"

/**************************************************************************************************
 * @brief Initializes the MPU and PLIC
 * *************************************************************************************************/
void bsp_init(void) {
    // Configure MPU regions
    // Allow access to all of DDR
    mpu_configure_region(0, DDR_BASE_ADDR, DDR_SIZE, PMP_READ | PMP_WRITE | PMP_EXEC, 0);
    // Allow access to AXI DMA registers
    mpu_configure_region(1, AXI_DMA_BASE_ADDR, 0x1000, PMP_READ | PMP_WRITE, 0);
    // Allow access to PLIC registers
    mpu_configure_region(2, PLIC_BASE_ADDR, 0x4000, PMP_READ | PMP_WRITE, 0);


    // Initialize PLIC
    plic_init();
}

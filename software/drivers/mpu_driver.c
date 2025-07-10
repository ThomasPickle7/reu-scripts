/**************************************************************************************************
 * @file mpu_driver.c
 * @author Thomas Pickle
 * @brief MPU Driver for the PolarFire SoC using PMP registers
 * @version 0.1
 * @date 2024-07-09
 * * @copyright Copyright (c) 2024
 * *************************************************************************************************/

#include "mpu_driver.h"

/**************************************************************************************************
 * @brief Configures a PMP region using NAPOT addressing
 * * @param region The PMP region to configure (0-7)
 * @param base The base address of the region
 * @param size The size of the region (must be a power of 2)
 * @param permissions The permissions for the region (PMP_READ, PMP_WRITE, PMP_EXEC)
 * @param lock The lock status of the region (PMP_LOCK)
 *************************************************************************************************/
void mpu_configure_region(uint8_t region, uint32_t base, uint32_t size, uint8_t permissions, uint8_t lock) {
    if (region > 7) {
        // Invalid region
        return;
    }

    // Calculate the NAPOT address
    uint32_t napot_address = base | ((size - 1) >> 1);

    // Configure the PMP address register
    __asm__ volatile ("csrw pmpaddr%0, %1" : : "i"(region), "r"(napot_address));

    // Configure the PMP configuration register
    uint8_t pmpcfg = permissions | PMP_NAPOT | lock;
    __asm__ volatile ("csrw pmpcfg%0, %1" : : "i"(region), "r"(pmpcfg));
}

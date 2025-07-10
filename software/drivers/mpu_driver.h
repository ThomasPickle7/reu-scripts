/**************************************************************************************************
 * @file mpu_driver.h
 * @author Thomas Pickle
 * @brief MPU Driver for the PolarFire SoC using PMP registers
 * @version 0.1
 * @date 2024-07-09
 * * @copyright Copyright (c) 2024
 * *************************************************************************************************/
#ifndef MPU_DRIVER_H_
#define MPU_DRIVER_H_

#include <stdint.h>

#define PMP_READ    0x01
#define PMP_WRITE   0x02
#define PMP_EXEC    0x04
#define PMP_LOCK    0x80

#define PMP_TOR     0x08
#define PMP_NA4     0x10
#define PMP_NAPOT   0x18

/**************************************************************************************************
 * @brief Configures a PMP region
 * * @param region The PMP region to configure
 * @param base The base address of the region
 * @param size The size of the region
 * @param permissions The permissions for the region (PMP_READ, PMP_WRITE, PMP_EXEC)
 * @param lock The lock status of the region (PMP_LOCK)
 *************************************************************************************************/
void mpu_configure_region(uint8_t region, uint32_t base, uint32_t size, uint8_t permissions, uint8_t lock);

#endif /* MPU_DRIVER_H_ */

/**************************************************************************************************
 * @file plic_driver.h
 * @author Thomas Pickle
 * @brief PLIC Driver for the PolarFire SoC
 * @version 0.1
 * @date 2024-07-09
 * * @copyright Copyright (c) 2024
 * *************************************************************************************************/

#ifndef PLIC_DRIVER_H_
#define PLIC_DRIVER_H_

#include "hw_platform.h"
#include <stdint.h>

/**************************************************************************************************
 * @brief Disables all interrupts
 * *************************************************************************************************/
void plic_init(void);

#endif /* PLIC_DRIVER_H_ */

/**************************************************************************************************
 * @file plic_driver.c
 * @author Thomas Pickle
 * @brief PLIC Driver for the PolarFire SoC
 * @version 0.1
 * @date 2024-07-09
 * * @copyright Copyright (c) 2024
 * *************************************************************************************************/

#include "plic_driver.h"

/**************************************************************************************************
 * @brief Disables all interrupts by writing 0 to the enable registers
 * *************************************************************************************************/
void plic_init(void) {
    /* Disable all interrupts */
    volatile uint32_t * p_plic_enable = (uint32_t *)PLIC_EN_OFFSET;
    p_plic_enable[0] = 0;
    p_plic_enable[1] = 0;
}

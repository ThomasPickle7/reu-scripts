#ifndef RADIOHOUND_DMA_API_H_
#define RADIOHOUND_DMA_API_H_

#include <stdint.h>
#include "dma_driver.h" // <-- FIX: Include the driver to get type definitions.

// Define the old type name as an alias for the new one for compatibility.
typedef Dma_Regs_t CoreAXI4DMAController_Regs_t;

/**************************************************************************************************
 * @brief Runs a memory-to-memory DMA test.
 *************************************************************************************************/
void rh_run_mem_test(
    CoreAXI4DMAController_Regs_t* dma_regs,
    int dma_uio_fd,
    uint64_t dma_phys_base,
    uint8_t* dma_virt_base
);

#endif // RADIOHOUND_DMA_API_H_

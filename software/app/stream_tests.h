#ifndef STREAM_TESTS_H_
#define STREAM_TESTS_H_

#include <stdint.h>
#include "dma_driver.h" // <-- FIX: Include the driver to get type definitions.

// Define the old type name as an alias for the new one for compatibility.
typedef Dma_Regs_t CoreAXI4DMAController_Regs_t;

/**************************************************************************************************
 * @brief Runs a stream-to-memory DMA test.
 *************************************************************************************************/
void run_stream_to_mem_test(
    CoreAXI4DMAController_Regs_t* dma_regs,
    int dma_uio_fd,
    uint64_t dma_phys_base,
    uint8_t* dma_virt_base
);

#endif // STREAM_TESTS_H_

#ifndef DIAGNOSTICS_H_
#define DIAGNOSTICS_H_

#include "dma_driver.h"

// Define the old type name as an alias
typedef Dma_Regs_t CoreAXI4DMAController_Regs_t;

/**************************************************************************************************
 * @brief Runs various diagnostic checks on the hardware.
 *************************************************************************************************/
void run_diagnostics(
    CoreAXI4DMAController_Regs_t* dma_regs,
    AxiStreamSource_Regs_t* stream_src_regs
);

#endif // DIAGNOSTICS_H_

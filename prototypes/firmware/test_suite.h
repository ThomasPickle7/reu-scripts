// =================================================================================================
// File: test_suite.h
// Description: Header for the various test and diagnostic functions.
// =================================================================================================

#ifndef TEST_SUITE_H
#define TEST_SUITE_H

#include "hw_platform.h"

// --- Function Prototypes ---

void run_axi_stream_source_test(
    Dma_Regs_t* dma_regs,
    AxiStreamSource_Regs_t* stream_src_regs,
    int dma_uio_fd,
    uintptr_t dma_phys_base,
    uint8_t* dma_virt_base
);

void run_diagnostics(Dma_Regs_t* dma_regs, AxiStreamSource_Regs_t* stream_src_regs);

void run_axi_lite_reg_test(AxiStreamSource_Regs_t* stream_src_regs);

#endif // TEST_SUITE_H



// =================================================================================================
// File: main.c
// Description: Main application entry point, initialization, and user menu.
// =================================================================================================

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>

#include "app_config.h"
#include "hw_platform.h"
#include "test_suite.h"

// --- Global Resource Handles ---
// Grouping these into a struct makes them easier to pass around.
typedef struct {
    int dma_uio_fd;
    int stream_src_uio_fd;
    int udma_buf_fd;
    Dma_Regs_t* dma_regs;
    AxiStreamSource_Regs_t* stream_src_regs;
    uint8_t* dma_virt_base;
    uintptr_t dma_phys_base;
    size_t dma_buffer_size;
} AppResources;


// --- Function Prototypes for main.c ---
void display_menu();
int initialize_system(AppResources* res);
void cleanup_system(AppResources* res);


// --- Main Application Logic ---
int main(void) {
    AppResources app = {
        .dma_uio_fd = -1,
        .stream_src_uio_fd = -1,
        .udma_buf_fd = -1,
        .dma_regs = NULL,
        .stream_src_regs = NULL,
        .dma_virt_base = NULL,
        .dma_phys_base = DMA_PHYSICAL_BASE_ADDR,
        .dma_buffer_size = DMA_BUFFER_SIZE
    };
    char choice;

    if (initialize_system(&app) != 0) {
        cleanup_system(&app); // Attempt to clean up partially initialized resources
        return -1;
    }

    // --- Main Application Loop ---
    while(1) {
        display_menu();
        scanf(" %c", &choice);
        while(getchar() != '\n'); // Clear input buffer

        if (choice == '1') {
            run_axi_stream_source_test(app.dma_regs, app.stream_src_regs, app.dma_uio_fd, app.dma_phys_base, app.dma_virt_base);
        } else if (choice == '2') {
            run_diagnostics(app.dma_regs, app.stream_src_regs);
        } else if (choice == '3') {
            run_axi_lite_reg_test(app.stream_src_regs);
        } else if (choice == 'Q' || choice == 'q') {
            break;
        } else {
            printf("Invalid option.\n");
        }
    }

    cleanup_system(&app);
    return 0;
}

void display_menu() {
    printf("\n# Choose one of the following options:\n");
    printf("  1 - Run Custom AXI Stream Source IP Test (Full DMA Test)\n");
    printf("  2 - Run Diagnostics\n");
    printf("  3 - Run AXI-Lite Register R/W Test\n");
    printf("  Q - Exit\n> ");
}

int initialize_system(AppResources* res) {
    printf("--- Initializing DMA and Peripherals ---\n");

    // Map DMA Controller
    res->dma_uio_fd = open(UIO_DMA_DEV_NAME, O_RDWR);
    if (res->dma_uio_fd < 0) {
        perror("Failed to open DMA UIO device");
        return -1;
    }
    res->dma_regs = mmap(NULL, 4096, PROT_READ | PROT_WRITE, MAP_SHARED, res->dma_uio_fd, 0);
    if (res->dma_regs == MAP_FAILED) {
        perror("Failed to mmap DMA registers");
        res->dma_regs = NULL; // Ensure pointer is NULL on failure
        return -1;
    }

    // Map AXI Stream Source
    res->stream_src_uio_fd = open(UIO_STREAM_SRC_DEV_NAME, O_RDWR);
    if (res->stream_src_uio_fd < 0) {
        perror("Failed to open Stream Source UIO device");
        return -1;
    }
    res->stream_src_regs = mmap(NULL, 4096, PROT_READ | PROT_WRITE, MAP_SHARED, res->stream_src_uio_fd, 0);
    if (res->stream_src_regs == MAP_FAILED) {
        perror("Failed to mmap Stream Source registers");
        res->stream_src_regs = NULL;
        return -1;
    }

    // Map UDMABuf for DMA
    res->udma_buf_fd = open(UDMABUF_DEVICE_NAME, O_RDWR);
    if (res->udma_buf_fd < 0) {
        perror("Failed to open udmabuf device");
        return -1;
    }
    res->dma_virt_base = mmap(NULL, res->dma_buffer_size, PROT_READ | PROT_WRITE, MAP_SHARED, res->udma_buf_fd, 0);
    if (res->dma_virt_base == MAP_FAILED) {
        perror("Failed to mmap udmabuf");
        res->dma_virt_base = NULL;
        return -1;
    }

    printf("Successfully mapped peripherals:\n");
    printf("  DMA Controller      (UIO): %s\n", UIO_DMA_DEV_NAME);
    printf("  AXI Stream Source   (UIO): %s\n", UIO_STREAM_SRC_DEV_NAME);
    printf("  DMA Buffer      (UDMABuf): %s (Size: %zu KB, Phys Addr: 0x%lX)\n",
           UDMABUF_DEVICE_NAME, res->dma_buffer_size / 1024, res->dma_phys_base);

    return 0; // Success
}

void cleanup_system(AppResources* res) {
    printf("\nCleaning up and exiting.\n");

    if (res->dma_regs) munmap((void*)res->dma_regs, 4096);
    if (res->stream_src_regs) munmap((void*)res->stream_src_regs, 4096);
    if (res->dma_virt_base) munmap(res->dma_virt_base, res->dma_buffer_size);

    if (res->dma_uio_fd != -1) close(res->dma_uio_fd);
    if (res->stream_src_uio_fd != -1) close(res->stream_src_uio_fd);
    if (res->udma_buf_fd != -1) close(res->udma_buf_fd);
}

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <errno.h>

#include "hw_platform.h"
#include "dma_driver.h"
#include "mpu_driver.h"
#include "radiohound_dma_api.h"
#include "stream_tests.h"
#include "diagnostics.h"

#define SYSFS_PATH_LEN 128
#define ID_STR_LEN 32
#define UIO_DEVICE_PATH_LEN 32
#define NUM_UIO_DEVICES 32
#define MAP_SIZE 4096UL


// --- Linux Platform Helpers ---
static int get_uio_device_number(const char *id) {
    // This function searches for a UIO device by its name in the sysfs filesystem.
    FILE *fp; 
    int i; 
    char file_id[ID_STR_LEN]; 
    char sysfs_path[SYSFS_PATH_LEN];
    for (i = 0; i < NUM_UIO_DEVICES; i++) {
        // Construct the sysfs path for the UIO device
        snprintf(sysfs_path, SYSFS_PATH_LEN, "/sys/class/uio/uio%d/name", i);
        fp = fopen(sysfs_path, "r"); if (fp == NULL) break;
        fscanf(fp, "%31s", file_id); fclose(fp);
        if (strncmp(file_id, id, strlen(id)) == 0) return i;
    }
    return -1;
}


static uint64_t get_udma_phys_addr(const char* uio_device_name) {
    char sysfs_path[SYSFS_PATH_LEN];
    FILE* fp;
    uint64_t paddr;
    
    snprintf(sysfs_path, SYSFS_PATH_LEN, "/sys/class/u-dma-buf/%s/phys_addr", uio_device_name);
    fp = fopen(sysfs_path, "r");
    if (fp == NULL) {
        fprintf(stderr, "Failed to open sysfs path '%s': ", sysfs_path);
        perror(NULL);
        return 0;
    }
    if (fscanf(fp, "%lx", &paddr) != 1) {
        fprintf(stderr, "Failed to read physical address from sysfs\n");
        paddr = 0;
    }
    fclose(fp);
    return paddr;
}


// --- Main Application ---

void display_menu() {
    printf("\n# Choose one of the following options:\n");
    printf("  1 - Run Memory-to-Memory Test\n");
    printf("  2 - Run Stream-to-Memory Test\n");
    printf("  3 - Run Diagnostics\n");
    printf("  Q - Exit\n> ");
}

int main(void) {
    int dma_uio_fd = -1, stream_src_uio_fd = -1, udma_buf_fd = -1;
    Dma_Regs_t *dma_regs = NULL;
    AxiStreamSource_Regs_t *stream_src_regs = NULL;
    uint8_t* dma_virt_base = NULL;
    uint64_t dma_phys_base = 0;
    size_t dma_buffer_size = STREAM_DESCRIPTOR_OFFSET + (NUM_BUFFERS * sizeof(DmaStreamDescriptor_t));
    char choice;

    printf("--- PolarFire SoC DMA Test Application ---\n");

    // This is a Linux userspace equivalent to configuring the MPU for the fabric bus
    if (!MPU_Configure_FIC0()) {
        fprintf(stderr, "Fatal: Could not configure MPU via /dev/mem. Halting.\n");
        return 1;
    }

    printf("\n--- Initializing Devices ---\n");
    
    // 1. Map DMA Controller
    int dma_uio_num = get_uio_device_number(UIO_DMA_DEVNAME);
    if (dma_uio_num < 0) { fprintf(stderr, "FATAL: Could not find UIO for %s.\n", UIO_DMA_DEVNAME); return 1; }
    char uio_dev_path[UIO_DEVICE_PATH_LEN];
    snprintf(uio_dev_path, UIO_DEVICE_PATH_LEN, "/dev/uio%d", dma_uio_num);
    dma_uio_fd = open(uio_dev_path, O_RDWR);
    if (dma_uio_fd < 0) { perror("FATAL: Failed to open DMA UIO"); return 1; }
    dma_regs = mmap(NULL, MAP_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, dma_uio_fd, 0);
    if (dma_regs == MAP_FAILED) { perror("FATAL: Failed to mmap DMA UIO"); close(dma_uio_fd); return 1; }
    
    // 2. Map Stream Source
    // NOTE: Add error handling as above
    int stream_src_uio_num = get_uio_device_number(UIO_STREAM_SRC_DEVNAME);
    snprintf(uio_dev_path, UIO_DEVICE_PATH_LEN, "/dev/uio%d", stream_src_uio_num);
    stream_src_uio_fd = open(uio_dev_path, O_RDWR);
    stream_src_regs = mmap(NULL, MAP_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, stream_src_uio_fd, 0);

    // 3. Map UDMA Buffer
    udma_buf_fd = open(UDMA_BUF_DEVNAME, O_RDWR | O_SYNC);
    if (udma_buf_fd < 0) { perror("FATAL: Failed to open " UDMA_BUF_DEVNAME); return 1; }
    dma_virt_base = mmap(NULL, dma_buffer_size, PROT_READ | PROT_WRITE, MAP_SHARED, udma_buf_fd, 0);
    if (dma_virt_base == MAP_FAILED) { perror("FATAL: Failed to mmap udmabuf"); return 1; }
    dma_phys_base = get_udma_phys_addr(UDMA_BUF_SYNC_DEVNAME);
    if (dma_phys_base == 0) { fprintf(stderr, "FATAL: Could not get physical address of udmabuf\n"); return 1; }

    printf("\n--- Initialization Complete ---\n");
    printf("DMA Controller Version: 0x%08X\n", dma_regs->VERSION_REG);

    while(1) {
        display_menu();
        scanf(" %c", &choice);
        while(getchar() != '\n'); // Clear input buffer

        if (choice == '1') {
            rh_run_mem_test(dma_regs, dma_uio_fd, dma_phys_base, dma_virt_base);
        } else if (choice == '2') {
            // FIX: Corrected the argument order to match the function prototype.
            // The stream test needs the DMA's UIO file descriptor for interrupts,
            // and the physical address before the virtual one.
            run_stream_to_mem_test(dma_regs, dma_uio_fd, dma_phys_base, dma_virt_base);
        } else if (choice == '3') {
            run_diagnostics(dma_regs, stream_src_regs);
        } else if (choice == 'Q' || choice == 'q') {
            break;
        } else {
            printf("Invalid option.\n");
        }
    }
    
    // Cleanup
    munmap(dma_virt_base, dma_buffer_size);
    close(udma_buf_fd);
    munmap(stream_src_regs, MAP_SIZE);
    close(stream_src_uio_fd);
    munmap(dma_regs, MAP_SIZE);
    close(dma_uio_fd);
    printf("\nExiting.\n");
    return 0;
}

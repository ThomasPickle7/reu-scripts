#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include "mpu_driver.h"
#include "hw_platform.h" // For MPU_BASE_ADDR

// Physical address of the MPU for the fabric interconnect (FIC0)
#define MPU_PHYS_BASE_ADDR         0x20005000UL
// Base address of the non-cached DDR memory region the MPU will grant access to
#define DDR_NON_CACHED_BASE_ADDR   0xC0000000UL


#define MAP_SIZE 4096UL
#define MAP_MASK (MAP_SIZE - 1)

int MPU_Configure_FIC0(void) {
    printf("--- Configuring MPU for FIC0 ---\n");

    int mem_fd = open("/dev/mem", O_RDWR | O_SYNC);
    if (mem_fd < 0) {
        perror("MPU Config: Failed to open /dev/mem");
        return 0;
    }

    // Map the MPU configuration block into this process's virtual memory
    void* map_base = mmap(0, MAP_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, mem_fd, MPU_PHYS_BASE_ADDR & ~MAP_MASK);
    if (map_base == MAP_FAILED) {
        perror("MPU Config: mmap failed");
        close(mem_fd);
        return 0;
    }
    // Get a pointer to the registers within the mapped page
    Mpu_Regs_t* mpu_regs = (Mpu_Regs_t*)((uint8_t*)map_base + (MPU_PHYS_BASE_ADDR & MAP_MASK));

    // Configure PMP0 for the Non-Cached DDR region (256MB at 0xC000_0000)
    // NAPOT mode address field is (base | (size - 1) >> 1)
    uint64_t pmp_addr = DDR_NON_CACHED_BASE_ADDR | (0x0FFFFFFF >> 1); // Range 0xC0000000 - 0xCFFFFFFF

    // The MODE field enables Read, Write, NAPOT matching, and Locks the entry.
    uint64_t pmp_mode = MPU_MODE_READ_EN | MPU_MODE_WRITE_EN | MPU_MODE_MATCH_NAPOT | MPU_MODE_LOCKED;

    MpuPmpEntry_t pmp_entry = pmp_addr | pmp_mode;

    printf("  - Writing PMPCFG[0] with value: 0x%016llx\n", (unsigned long long)pmp_entry);
    mpu_regs->PMPCFG[0] = pmp_entry;

    __sync_synchronize(); // Memory barrier to ensure write completes

    // Verify the write
    if (mpu_regs->PMPCFG[0] == pmp_entry) {
        printf("  - MPU PMPCFG0 successfully configured.\n");
    } else {
        printf("  - MPU PMPCFG0 configuration FAILED. Read back 0x%016llx\n", (unsigned long long)mpu_regs->PMPCFG[0]);
    }

    munmap(map_base, MAP_SIZE);
    close(mem_fd);

    printf("--- MPU Configuration Complete ---\n");
    return 1;
}

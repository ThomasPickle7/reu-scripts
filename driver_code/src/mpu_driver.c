#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include "mpu_driver.h"
#include "hw_platform.h"

#define MAP_SIZE 4096UL
#define MAP_MASK (MAP_SIZE - 1)

int MPU_Configure_FIC0(void) {
    printf("--- Configuring MPU for FIC0 ---\n");

    int mem_fd = open("/dev/mem", O_RDWR | O_SYNC);
    if (mem_fd < 0) {
        perror("MPU Config: Failed to open /dev/mem");
        return 0;
    }

    // Map the MPU configuration block into virtual memory
    void* map_base = mmap(0, MAP_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, mem_fd, MPU_BASE_ADDR & ~MAP_MASK);
    if (map_base == MAP_FAILED) {
        perror("MPU Config: mmap failed");
        close(mem_fd);
        return 0;
    }
    Mpu_Regs_t* mpu_regs = (Mpu_Regs_t*)((uint8_t*)map_base + (MPU_BASE_ADDR & MAP_MASK));

    // We will configure the first PMP entry (PMP0) to grant full access to the
    // entire 4GB DDR address space for simplicity in this debug scenario.
    // The address is 0x80000000 and the size is 2GB (2^31).
    // In NAPOT mode, this is encoded by setting the address to the base and the
    // size mask to (2^31 - 1), which is 0x7FFFFFFF.
    // The final address field is (base | (size - 1) >> 1)
    uint64_t pmp_addr = 0x80000000 | (0x7FFFFFFF >> 1);

    // The MODE field enables Read, Write, NAPOT matching, and Locks the entry.
    uint64_t pmp_mode = MPU_MODE_READ_EN | MPU_MODE_WRITE_EN | MPU_MODE_MATCH_NAPOT | MPU_MODE_LOCKED;
    
    // Combine into a single 64-bit value to write to the PMPCFG0 register
    MpuPmpEntry_t pmp_entry = pmp_addr | pmp_mode;

    mpu_regs->PMPCFG[0] = pmp_entry;
    
    // Read back to verify
    if (mpu_regs->PMPCFG[0] == pmp_entry) {
        printf("  - MPU PMPCFG0 successfully configured to grant access to DDR.\n");
    } else {
        printf("  - MPU PMPCFG0 configuration FAILED.\n");
    }

    munmap(map_base, MAP_SIZE);
    close(mem_fd);

    printf("--- MPU Configuration Complete ---\n");
    return 1;
}

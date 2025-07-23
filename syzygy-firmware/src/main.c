#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <stdint.h> // For uint32_t

// --- Memory Map Definitions ---

// Base address for the HIGH_SPEED_INTERFACE component (from Libero Memory Map report)
#define HSI_PHYSICAL_BASE_ADDR 0x44000000
// Offset for the PRESCALE register within the CorePWM module (from CorePWM Handbook)
#define PWM_PRESCALE_REGISTER_OFFSET 0x00
// Expected default value of the PRESCALE register (from CorePWM Handbook)
#define PWM_PRESCALE_DEFAULT_VALUE 0x08

// Base address for the System Controller Registers (SYSREG)
// Source: PolarFire SoC FPGA MSS Technical Reference Manual
#define SYSREG_PHYSICAL_BASE_ADDR 0x20003000

// Offset for the SYSREG Control register
#define SYSREG_CTRL_OFFSET 0x00
// Bitmask for the LOCK bit in the control register
#define SYSREG_LOCK_MASK (1 << 0)

// Offset for the Sub-Block Clock Enable register
#define SUBBLKCKEN_OFFSET 0x08
// Bitmask to enable the FIC3 clock (Fabric Interface Controller 3)
#define FIC3_CLK_EN_MASK (1 << 11)

// Offset for the Software Reset Control Register
#define SOFT_RESET_CR_OFFSET 0x44
// Bitmask for the active-low FPGA fabric reset
#define FPGA_RESET_N_MASK (1 << 0)

// Standard page size for memory mapping
#define MAP_SIZE 4096UL
#define MAP_MASK (MAP_SIZE - 1)

// Helper function to de-assert fabric reset and enable the FIC3 clock.
int initialize_fabric_interface() {
    int fd;
    void *map_base;
    volatile uint32_t *ctrl_reg, *clk_en_reg, *reset_reg;
    uint32_t clk_en_val;

    if ((fd = open("/dev/mem", O_RDWR | O_SYNC)) == -1) {
        perror("Error opening /dev/mem for fabric init");
        return -1;
    }

    map_base = mmap(0, MAP_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, SYSREG_PHYSICAL_BASE_ADDR & ~MAP_MASK);
    if (map_base == (void *) -1) {
        perror("Error mapping SYSREG memory");
        close(fd);
        return -1;
    }

    // Get pointers to all necessary system registers
    ctrl_reg = (volatile uint32_t *)(map_base + (SYSREG_PHYSICAL_BASE_ADDR & MAP_MASK) + SYSREG_CTRL_OFFSET);
    clk_en_reg = (volatile uint32_t *)(map_base + (SYSREG_PHYSICAL_BASE_ADDR & MAP_MASK) + SUBBLKCKEN_OFFSET);
    reset_reg = (volatile uint32_t *)(map_base + (SYSREG_PHYSICAL_BASE_ADDR & MAP_MASK) + SOFT_RESET_CR_OFFSET);

    clk_en_val = *clk_en_reg;
    printf("SYSREG: Current SUBBLKCKEN value: 0x%08X\n", clk_en_val);

    if (!(clk_en_val & FIC3_CLK_EN_MASK)) {
        printf("SYSREG: FIC3 clock is disabled. Initializing fabric interface...\n");

        // --- UNLOCK -> DE-ASSERT RESET -> ENABLE CLOCK -> RE-LOCK ---
        *ctrl_reg &= ~SYSREG_LOCK_MASK;         // Unlock SYSREG
        *reset_reg |= FPGA_RESET_N_MASK;        // De-assert FPGA reset (set bit to 1)
        *clk_en_reg |= FIC3_CLK_EN_MASK;        // Enable the FIC3 clock
        *ctrl_reg |= SYSREG_LOCK_MASK;          // Re-lock the SYSREG
        
        printf("SYSREG: New SUBBLKCKEN value: 0x%08X\n", *clk_en_reg);
        printf("SYSREG: New SOFT_RESET_CR value: 0x%08X\n", *reset_reg);
    } else {
        printf("SYSREG: FIC3 clock is already enabled.\n");
    }

    munmap(map_base, MAP_SIZE);
    close(fd);
    return 0;
}


int main() {
    int fd;
    void *map_base, *virt_addr;
    uint32_t read_value;

    printf("--- Starting Sanity Check for HIGH_SPEED_INTERFACE ---\n");

    // --- Step 1: Initialize the MSS-to-Fabric interface ---
    if (initialize_fabric_interface() != 0) {
        fprintf(stderr, "Failed to initialize fabric interface. Aborting.\n");
        return 1;
    }

    // --- Step 2: Access the custom peripheral ---
    if ((fd = open("/dev/mem", O_RDWR | O_SYNC)) == -1) {
        perror("Error opening /dev/mem for HSI access");
        return 1;
    }
    printf("Successfully opened /dev/mem for HSI access.\n");

    map_base = mmap(0, MAP_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, HSI_PHYSICAL_BASE_ADDR & ~MAP_MASK);
    if (map_base == (void *) -1) {
        perror("Error mapping HSI memory");
        close(fd);
        return 1;
    }
    printf("HSI memory mapped successfully at virtual address %p\n", map_base);

    virt_addr = map_base + (HSI_PHYSICAL_BASE_ADDR & MAP_MASK) + PWM_PRESCALE_REGISTER_OFFSET;
    volatile uint32_t *prescale_reg = (volatile uint32_t *)virt_addr;

    read_value = *prescale_reg;

    printf("Reading from PRESCALE register at physical address 0x%X...\n", HSI_PHYSICAL_BASE_ADDR);
    printf("Value read: 0x%X\n", read_value);

    if (read_value == PWM_PRESCALE_DEFAULT_VALUE) {
        printf("Sanity Check PASSED! Communication with the PWM core is successful.\n");
    } else {
        printf("Sanity Check FAILED! Expected 0x%X but got 0x%X.\n", PWM_PRESCALE_DEFAULT_VALUE, read_value);
    }

    munmap(map_base, MAP_SIZE);
    close(fd);

    return 0;
}

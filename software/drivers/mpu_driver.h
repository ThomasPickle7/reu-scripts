#ifndef MPU_DRIVER_H_
#define MPU_DRIVER_H_

#include <stdint.h>

/**
 * @brief Structure for a single PMP (Physical Memory Protection) entry within an MPU.
 * A 64-bit register combining address and configuration.
 */
typedef volatile uint64_t MpuPmpEntry_t;

/**
 * @brief Defines the register map for a single MPU (for FIC0).
 */
typedef struct {
    MpuPmpEntry_t PMPCFG[16]; // 16 PMP entries per MPU for FIC0
    uint8_t       _RESERVED[0x80 - (sizeof(MpuPmpEntry_t) * 16)];
    volatile const uint64_t STATUS;
} Mpu_Regs_t;


// Bit definitions for the PMPCFG register's MODE field
#define MPU_MODE_READ_EN        (1ULL << 56)
#define MPU_MODE_WRITE_EN       (1ULL << 57)
#define MPU_MODE_EXEC_EN        (1ULL << 58)
#define MPU_MODE_MATCH_NAPOT    (3ULL << 59)
#define MPU_MODE_LOCKED         (1ULL << 63)

/**
 * @brief Configures MPU1 (for FIC0) to allow full access to all of DDR memory.
 * This must be called at startup to allow the fabric DMA to work.
 * @return 1 on success, 0 on failure.
 */
int MPU_Configure_FIC0(void);

#endif /* MPU_DRIVER_H_ */

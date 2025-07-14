#ifndef DMA_DRIVER_H_
#define DMA_DRIVER_H_

#include <stdint.h>

// --- Hardware Register Structures ---

// Structure for a single Memory-Mapped DMA descriptor block
typedef struct {
    volatile uint32_t CONFIG_REG;
    volatile uint32_t BYTE_COUNT_REG;
    volatile uint32_t SOURCE_ADDR_REG;
    volatile uint32_t DEST_ADDR_REG;
    volatile uint32_t NEXT_DESC_ADDR_REG;
    uint8_t           _RESERVED[0x20 - 0x14];
} DmaDescriptorBlock_t;

// Structure for a Stream-based DMA descriptor
typedef struct {
    volatile uint32_t CONFIG_REG;
    volatile uint32_t BYTE_COUNT_REG;
    volatile uint32_t DEST_ADDR_REG;
} DmaStreamDescriptor_t;

// Structure for the CoreAXI4DMAController IP
typedef struct {
    volatile const uint32_t VERSION_REG;
    volatile uint32_t       START_OPERATION_REG;
    uint8_t                 _RESERVED1[0x10 - 0x08];
    volatile const uint32_t INTR_0_STAT_REG;
    volatile uint32_t       INTR_0_MASK_REG;
    volatile uint32_t       INTR_0_CLEAR_REG;
    uint8_t                 _RESERVED2[0x60 - 0x1C];
    DmaDescriptorBlock_t    DESCRIPTOR[32];
    volatile uint32_t       STREAM_ADDR_REG[4];
} Dma_Regs_t;

// Structure for the AXI4StreamMaster IP Core Registers
typedef struct {
    volatile uint32_t CONTROL_REG;
    volatile const uint32_t STATUS_REG;
    uint8_t           _RESERVED1[0x10 - 0x08];
    volatile uint32_t NUM_BYTES_REG;
    volatile uint32_t DEST_REG;
} AxiStreamSource_Regs_t;


// --- Bitfield Flags ---

// For Memory-Mapped Descriptors
#define MEM_OP_INCR              (0b01)
#define MEM_FLAG_CHAIN           (1U << 10)
#define MEM_FLAG_IRQ_ON_PROCESS  (1U << 12)
#define MEM_FLAG_SRC_RDY         (1U << 13)
#define MEM_FLAG_DEST_RDY        (1U << 14)
#define MEM_FLAG_VALID           (1U << 15)
#define MEM_CONF_BASE            ((MEM_OP_INCR << 2) | MEM_OP_INCR | MEM_FLAG_CHAIN | MEM_FLAG_IRQ_ON_PROCESS)

// For Stream Descriptors
#define STREAM_OP_INCR           (0b01)
#define STREAM_FLAG_CHAIN        (1U << 1)
#define STREAM_FLAG_DEST_RDY     (1U << 2)
#define STREAM_FLAG_VALID        (1U << 3)
#define STREAM_FLAG_IRQ_EN       (1U << 4)
#define STREAM_CONF_BASE         (STREAM_OP_INCR | STREAM_FLAG_IRQ_EN)

// For DMA Control
#define FDMA_START_MEM(n)        (1U << (n))
#define FDMA_START_STREAM(n)     (1U << (16 + n))
#define FDMA_IRQ_MASK_ALL        (0x0FU)
#define FDMA_IRQ_CLEAR_ALL       (0x0FU)
#define FDMA_IRQ_STAT_WR_ERR     (1U << 1)
#define FDMA_IRQ_STAT_INVALID_DESC (1U << 3)


// --- Function Prototypes for dma_driver.c ---

void dma_force_stop(Dma_Regs_t* dma_regs);
void dma_reset_interrupts(Dma_Regs_t* dma_regs, int dma_uio_fd);

#endif // DMA_DRIVER_H_

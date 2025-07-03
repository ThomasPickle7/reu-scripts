/*******************************************************************************
 *******************************************************************************
 *
 * FILE: axi_dma_driver.c
 *
 * DESCRIPTION:
 * Implementation of the CoreAXI4DMAController driver.
 *
 *******************************************************************************
 *******************************************************************************/
#include "axi_dma_driver.h"
#include <stdio.h>
#include <string.h>
#include "mpfs_hal/mss_hal.h"

/*------------------------------------------------------------------------------
 * Private Driver Defines
 */
#define MAX_DMA_CONTROLLERS             4u

// Register Offsets from DMA Base Address
#define AXI_DMA_START_REGISTER          0x04

// Descriptor Offsets from DMA Base Address
#define DESC_0_OFFSET                   0x60
#define DESC_1_OFFSET                   0x80

// Register Offsets within a Descriptor
#define DESC_CONFIG_REG_OFFSET          0x00
#define DESC_BYTE_COUNT_REG_OFFSET      0x04
#define DESC_SRC_ADDR_REG_OFFSET        0x08
#define DESC_DEST_ADDR_REG_OFFSET       0x0C
#define DESC_NEXT_DESC_ADDR_REG_OFFSET  0x10

// Bitmasks for the Descriptor Configuration Register
#define DESC_CONFIG_CHAIN_MASK          (1u << 10)
#define DESC_CONFIG_INTR_ON_PROC_MASK   (1u << 12)
#define DESC_CONFIG_DATA_READY_MASK     (1u << 14)
#define DESC_CONFIG_VALID_MASK          (1u << 15)


/*------------------------------------------------------------------------------
 * Private Driver Data
 */
static const uint32_t g_dma_base_addr[MAX_DMA_CONTROLLERS] = {
    DMA0_BASE_ADDR, DMA1_BASE_ADDR, DMA2_BASE_ADDR, DMA3_BASE_ADDR
};

static volatile uint32_t* g_dma_start_reg[MAX_DMA_CONTROLLERS];
volatile uint32_t g_dma_completed_buffer_flag = 0;

/*------------------------------------------------------------------------------
 * Driver Functions
 */
void dma_init(void)
{
    for (uint8_t i = 0; i < MAX_DMA_CONTROLLERS; i++)
    {
        g_dma_start_reg[i] = (uint32_t *)(g_dma_base_addr[i] + AXI_DMA_START_REGISTER);
    }
}

void dma_configure_continuous_stream(uint8_t dma_id)
{
    if (dma_id >= MAX_DMA_CONTROLLERS)
    {
        return; // Invalid DMA ID
    }

    uint32_t dma_base = g_dma_base_addr[dma_id];
    volatile uint32_t* p_desc0 = (uint32_t*)(dma_base + DESC_0_OFFSET);
    volatile uint32_t* p_desc1 = (uint32_t*)(dma_base + DESC_1_OFFSET);
    uint32_t desc_config;

    // --- Configure Descriptor 0 (for Buffer A) ---
    p_desc0[DESC_DEST_ADDR_REG_OFFSET / 4]  = STREAM_BUFFER_A_ADDR;
    p_desc0[DESC_BYTE_COUNT_REG_OFFSET / 4] = STREAM_CHUNK_SIZE;
    p_desc0[DESC_NEXT_DESC_ADDR_REG_OFFSET / 4] = 1; // Chain to Descriptor 1

    desc_config = DESC_CONFIG_CHAIN_MASK | DESC_CONFIG_INTR_ON_PROC_MASK |
                  DESC_CONFIG_DATA_READY_MASK | DESC_CONFIG_VALID_MASK;
    p_desc0[DESC_CONFIG_REG_OFFSET / 4] = desc_config;

    // --- Configure Descriptor 1 (for Buffer B) ---
    p_desc1[DESC_DEST_ADDR_REG_OFFSET / 4]  = STREAM_BUFFER_B_ADDR;
    p_desc1[DESC_BYTE_COUNT_REG_OFFSET / 4] = STREAM_CHUNK_SIZE;
    p_desc1[DESC_NEXT_DESC_ADDR_REG_OFFSET / 4] = 0; // Chain back to Descriptor 0

    desc_config = DESC_CONFIG_CHAIN_MASK | DESC_CONFIG_INTR_ON_PROC_MASK |
                  DESC_CONFIG_DATA_READY_MASK | DESC_CONFIG_VALID_MASK;
    p_desc1[DESC_CONFIG_REG_OFFSET / 4] = desc_config;
}

void dma_start_transfer(uint8_t dma_id, uint8_t descriptor_id)
{
    if (dma_id < MAX_DMA_CONTROLLERS)
    {
        *g_dma_start_reg[dma_id] = (1u << descriptor_id);
    }
}

uint8_t fabric_f2h_6_plic_IRQHandler(void)
{
    g_dma_completed_buffer_flag = 1;
    // In a real application, you would also read the DMA's interrupt status
    // register here to see which descriptor finished and then clear the interrupt.
    return EXT_IRQ_KEEP_ENABLED;
}


/**************************************************************************************************
 * @file main.c
 * @author Thomas Pickell
 * @brief Main application for demonstrating and testing the Radio-Hound project's DMA system.
 * @version 0.2
 * @date 2024-07-15
 * @copyright Copyright (c) 2024
 * *************************************************************************************************/

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <string.h>

//=================================================================================================
// hw_platform.h Content
//=================================================================================================

#define UIO_DMA_DEVNAME "/dev/uio0"
#define UIO_STREAM_SRC_DEVNAME "/dev/uio1"
#define UDMABUF_DEVICE "/dev/udmabuf-ddr-nc0"

typedef volatile struct {
    uint32_t CONTROL_REG;
    uint32_t STATUS_REG;
    uint32_t RESERVED1[2];
    uint32_t NUM_BYTES_REG;
    uint32_t DEST_REG;
} AxiStreamSource_Regs_t;

//=================================================================================================
// dma_driver.h Content
//=================================================================================================

#define FDMA_MAX_STREAMS 4
#define FDMA_MAX_MEMORY_WINDOWS 4

#define FDMA_START_MEM_READ(win)  (1 << (win + 0))
#define FDMA_START_MEM_WRITE(win) (1 << (win + 4))
#define FDMA_START_STREAM(id)     (1 << (id + 8))

#define FDMA_IRQ_MASK_MEM_READ_DONE(win)  (1 << (win + 0))
#define FDMA_IRQ_MASK_MEM_WRITE_DONE(win) (1 << (win + 4))
#define FDMA_IRQ_MASK_STREAM_DONE(id)     (1 << (id + 8))
#define FDMA_IRQ_MASK_ALL 0xFFFFFFFF

typedef volatile struct {
    uint32_t ID_REG;
    uint32_t CONFIG_REG;
    uint32_t START_OPERATION_REG;
    uint32_t MPU_PROTECT_REG[FDMA_MAX_MEMORY_WINDOWS];
    uint32_t INTR_0_STAT_REG;
    uint32_t INTR_0_MASK_REG;
    uint32_t INTR_1_STAT_REG;
    uint32_t INTR_1_MASK_REG;
    uint32_t STREAM_ADDR_REG[FDMA_MAX_STREAMS];
    uint32_t MEM_READ_ADDR_REG[FDMA_MAX_MEMORY_WINDOWS];
    uint32_t MEM_READ_BYTE_COUNT_REG[FDMA_MAX_MEMORY_WINDOWS];
    uint32_t MEM_WRITE_ADDR_REG[FDMA_MAX_MEMORY_WINDOWS];
    uint32_t MEM_WRITE_BYTE_COUNT_REG[FDMA_MAX_MEMORY_WINDOWS];
} Dma_Regs_t;

typedef volatile struct {
    uint32_t SRC_ADDR_REG;
    uint32_t DEST_ADDR_REG;
    uint32_t BYTE_COUNT_REG;
    uint32_t CONFIG_REG;
} DmaStreamDescriptor_t;

#define STREAM_OP_FIXED      (0 << 0)
#define STREAM_OP_INCR       (1 << 0)
#define STREAM_FLAG_IRQ_EN   (1 << 1)
#define STREAM_FLAG_DEST_RDY (1 << 2)
#define STREAM_FLAG_VALID    (1 << 3)

void dma_reset_interrupts(Dma_Regs_t* dma_regs, int uio_fd);
void force_dma_stop(Dma_Regs_t* dma_regs);

//=================================================================================================
// dma_driver.c Content
//=================================================================================================

void dma_reset_interrupts(Dma_Regs_t* dma_regs, int uio_fd) {
    dma_regs->INTR_0_MASK_REG = 0;
    dma_regs->INTR_0_STAT_REG = 0xFFFFFFFF;
    __sync_synchronize();

    uint32_t irq_count;
    lseek(uio_fd, 0, SEEK_SET);
    read(uio_fd, &irq_count, sizeof(irq_count));
}

void force_dma_stop(Dma_Regs_t* dma_regs) {
    dma_regs->START_OPERATION_REG = 0;
    dma_regs->CONFIG_REG &= ~1;
    __sync_synchronize();
    dma_regs->CONFIG_REG |= 1;
    __sync_synchronize();
    printf("DMA Controller Reset.\n");
}


//=================================================================================================
// Test Function Prototypes and Data Offsets
//=================================================================================================
#define STREAM_DESCRIPTOR_OFFSET 0x0
#define STREAM_DEST_OFFSET       0x1000
#define NUM_BUFFERS              1

void run_axi_stream_source_test(
    Dma_Regs_t* dma_regs, 
    AxiStreamSource_Regs_t* stream_src_regs, 
    int dma_uio_fd, 
    uintptr_t dma_phys_base, 
    uint8_t* dma_virt_base
);

void run_diagnostics(Dma_Regs_t* dma_regs, AxiStreamSource_Regs_t* stream_src_regs);

//=================================================================================================
// stream_tests.c Content
//=================================================================================================

void run_axi_stream_source_test(
    Dma_Regs_t* dma_regs, 
    AxiStreamSource_Regs_t* stream_src_regs, 
    int dma_uio_fd, 
    uintptr_t dma_phys_base, 
    uint8_t* dma_virt_base) 
{
    printf("\n--- Running Custom AXI Stream Source -> DDR Test ---\n");

    const size_t test_size = 4096; // Transfer 4KB of data
    int test_passed = 1;

    // 1. Reset DMA and interrupts to a clean state
    dma_reset_interrupts(dma_regs, dma_uio_fd);

    // 2. Prepare the destination buffer in DDR memory
    uint8_t* virt_dest_buf = dma_virt_base + STREAM_DEST_OFFSET;
    uintptr_t phys_dest_buf = dma_phys_base + STREAM_DEST_OFFSET;
    memset(virt_dest_buf, 0, test_size); // Clear the destination buffer
    printf("  Destination DDR buffer prepared at physical address 0x%lX\n", phys_dest_buf);

    // 3. Configure a single stream descriptor in DMA-accessible memory
    DmaStreamDescriptor_t* stream_descriptor = (DmaStreamDescriptor_t*)(dma_virt_base + STREAM_DESCRIPTOR_OFFSET);
    stream_descriptor->DEST_ADDR_REG  = phys_dest_buf;
    stream_descriptor->BYTE_COUNT_REG = test_size;
    stream_descriptor->CONFIG_REG = STREAM_OP_INCR | STREAM_FLAG_IRQ_EN | STREAM_FLAG_DEST_RDY | STREAM_FLAG_VALID;
    
    uintptr_t phys_desc_addr = dma_phys_base + STREAM_DESCRIPTOR_OFFSET;
    printf("  Stream descriptor configured at physical address 0x%lX\n", phys_desc_addr);

    // 4. Point the DMA's stream channel to our descriptor
    dma_regs->STREAM_ADDR_REG[0] = phys_desc_addr;
    dma_regs->INTR_0_MASK_REG = FDMA_IRQ_MASK_STREAM_DONE(0); // Unmask interrupt for stream 0
    __sync_synchronize(); 

    // 5. Start the DMA stream channel (it will now wait for the stream)
    dma_regs->START_OPERATION_REG = FDMA_START_STREAM(0);
    printf("  DMA Stream Channel 0 started. Waiting for data...\n");

    // 6. Configure and start the AXI Stream Source module
    printf("  Configuring AXI Stream Source to send %zu bytes...\n", test_size);
    stream_src_regs->NUM_BYTES_REG = test_size;
    stream_src_regs->DEST_REG = 0; // TDEST value
    stream_src_regs->CONTROL_REG = 1; // Assert start bit
    __sync_synchronize();
    stream_src_regs->CONTROL_REG = 0; // De-assert start (it's a pulse)
    printf("  AXI Stream Source started.\n");

    // 7. Wait for the DMA completion interrupt
    uint32_t irq_count;
    printf("  Waiting for DMA completion interrupt...\n");
    read(dma_uio_fd, &irq_count, sizeof(irq_count));
    uint32_t status = dma_regs->INTR_0_STAT_REG;
    printf("  Interrupt received! DMA Status Register: 0x%08X\n", status);
    
    // 8. Verify the received data
    printf("  Verifying received data...\n");
    for (size_t i = 0; i < test_size / 4; ++i) {
        uint32_t expected_data = i; // The verilog module sends an incrementing pattern
        uint32_t actual_data = ((uint32_t*)virt_dest_buf)[i];
        if (expected_data != actual_data) {
            printf("  ERROR: Data mismatch at offset 0x%zX! Expected: 0x%08X, Got: 0x%08X\n",
                   i * 4, expected_data, actual_data);
            test_passed = 0;
            break;
        }
    }

    if (test_passed) {
        printf("\n***** AXI Stream Source Test PASSED *****\n");
    } else {
        printf("\n***** AXI Stream Source Test FAILED *****\n");
    }

    // Cleanup
    force_dma_stop(dma_regs);
    dma_reset_interrupts(dma_regs, dma_uio_fd);
}

//=================================================================================================
// diagnostics.c Content
//=================================================================================================
void run_diagnostics(Dma_Regs_t* dma_regs, AxiStreamSource_Regs_t* stream_src_regs) {
    printf("\n--- Running Diagnostics ---\n");
    
    if (dma_regs) {
        printf("DMA Controller Registers:\n");
        printf("  ID_REG: 0x%08X\n", dma_regs->ID_REG);
        printf("  CONFIG_REG: 0x%08X\n", dma_regs->CONFIG_REG);
        printf("  INTR_0_STAT_REG: 0x%08X\n", dma_regs->INTR_0_STAT_REG);
        printf("  INTR_0_MASK_REG: 0x%08X\n", dma_regs->INTR_0_MASK_REG);
    } else {
        printf("DMA Controller not mapped.\n");
    }

    if (stream_src_regs) {
        printf("AXI Stream Source Registers:\n");
        printf("  CONTROL_REG: 0x%08X\n", stream_src_regs->CONTROL_REG);
        printf("  STATUS_REG: 0x%08X\n", stream_src_regs->STATUS_REG);
        printf("  NUM_BYTES_REG: 0x%08X\n", stream_src_regs->NUM_BYTES_REG);
        printf("  DEST_REG: 0x%08X\n", stream_src_regs->DEST_REG);
    } else {
        printf("AXI Stream Source not mapped.\n");
    }
    printf("-------------------------\n");
}


//=================================================================================================
// Main Application
//=================================================================================================

void display_menu() {
    printf("\n# Choose one of the following options:\n");
    printf("  1 - Run Custom AXI Stream Source IP Test (Full DMA Test)\n");
    printf("  2 - Run Diagnostics\n");
    printf("  3 - Run AXI-Lite Register R/W Test\n"); // New Option
    printf("  Q - Exit\n> ");
}




/**************************************************************************************************
 * @brief Runs a simple read/write test on the AXI Stream Source's control registers.
 *************************************************************************************************/
void run_axi_lite_reg_test(AxiStreamSource_Regs_t* stream_src_regs) {
    printf("\n--- Running AXI-Lite Register Test ---\n");
    
    // Choose a test value to write
    const uint32_t test_value = 0xDEADBEEF;
    int test_passed = 1;

    printf("  Writing 0x%08X to NUM_BYTES_REG (Offset 0x10)...\n", test_value);
    stream_src_regs->NUM_BYTES_REG = test_value;
    __sync_synchronize(); // Ensure the write completes

    printf("  Reading back from NUM_BYTES_REG...\n");
    uint32_t read_value = stream_src_regs->NUM_BYTES_REG;
    printf("  Read value: 0x%08X\n", read_value);

    if (read_value == test_value) {
        printf("  Read/Write test for NUM_BYTES_REG PASSED!\n");
    } else {
        printf("  ERROR: Read/Write test for NUM_BYTES_REG FAILED!\n");
        test_passed = 0;
    }

    // Now test the DEST_REG
    const uint32_t test_value2 = 0x12345678;
    printf("\n  Writing 0x%08X to DEST_REG (Offset 0x14)...\n", test_value2);
    stream_src_regs->DEST_REG = test_value2;
     __sync_synchronize();

    printf("  Reading back from DEST_REG...\n");
    uint32_t read_value2 = stream_src_regs->DEST_REG;
    printf("  Read value: 0x%08X\n", read_value2);

    if (read_value2 == test_value2) {
        printf("  Read/Write test for DEST_REG PASSED!\n");
    } else {
        printf("  ERROR: Read/Write test for DEST_REG FAILED!\n");
        test_passed = 0;
    }

    if(test_passed) {
        printf("\n***** Basic AXI-Lite communication appears to be WORKING. *****\n");
    } else {
        printf("\n***** Basic AXI-Lite communication FAILED. Check FPGA design. *****\n");
    }
}



int main(void) {
    int dma_uio_fd = -1, stream_src_uio_fd = -1, udma_buf_fd = -1;
    Dma_Regs_t *dma_regs = NULL;
    AxiStreamSource_Regs_t *stream_src_regs = NULL;
    uint8_t* dma_virt_base = NULL;
    // We discovered the physical address from the dmesg log!
    uint64_t dma_phys_base = 0xc8000000; 
    size_t dma_buffer_size = STREAM_DEST_OFFSET + 8192; // 8K buffer for data + descriptors
    char choice;
    char* udma_buf_dev_name = "/dev/udmabuf-ddr-nc0"; // From dmesg log

    // --- UIO Device and Memory Mapping ---
    printf("--- Initializing DMA and Peripherals ---\n");

    // Map DMA Controller
    dma_uio_fd = open(UIO_DMA_DEVNAME, O_RDWR);
    if (dma_uio_fd < 0) {
        perror("Failed to open DMA UIO device");
        return -1;
    }
    dma_regs = mmap(NULL, 4096, PROT_READ | PROT_WRITE, MAP_SHARED, dma_uio_fd, 0);
    if (dma_regs == MAP_FAILED) {
        perror("Failed to mmap DMA registers");
        close(dma_uio_fd);
        return -1;
    }

    // Map AXI Stream Source
    stream_src_uio_fd = open(UIO_STREAM_SRC_DEVNAME, O_RDWR);
    if (stream_src_uio_fd < 0) {
        perror("Failed to open Stream Source UIO device");
        munmap((void*)dma_regs, 4096);
        close(dma_uio_fd);
        return -1;
    }
    stream_src_regs = mmap(NULL, 4096, PROT_READ | PROT_WRITE, MAP_SHARED, stream_src_uio_fd, 0);
    if (stream_src_regs == MAP_FAILED) {
        perror("Failed to mmap Stream Source registers");
        close(dma_uio_fd);
        close(stream_src_uio_fd);
        munmap((void*)dma_regs, 4096);
        return -1;
    }

    // Map UDMABuf for DMA
    udma_buf_fd = open(udma_buf_dev_name, O_RDWR);
    if (udma_buf_fd < 0) {
        perror("Failed to open udmabuf device");
        // Cleanup other resources
        munmap((void*)dma_regs, 4096);
        munmap((void*)stream_src_regs, 4096);
        close(dma_uio_fd);
        close(stream_src_uio_fd);
        return -1;
    }
    dma_virt_base = mmap(NULL, dma_buffer_size, PROT_READ | PROT_WRITE, MAP_SHARED, udma_buf_fd, 0);
    if (dma_virt_base == MAP_FAILED) {
        perror("Failed to mmap udmabuf");
        // Cleanup all resources
        close(udma_buf_fd);
        munmap((void*)dma_regs, 4096);
        munmap((void*)stream_src_regs, 4096);
        close(dma_uio_fd);
        close(stream_src_uio_fd);
        return -1;
    }
    
    printf("Successfully mapped peripherals:\n");
    printf("  DMA Controller       (UIO): %s\n", UIO_DMA_DEVNAME);
    printf("  AXI Stream Source    (UIO): %s\n", UIO_STREAM_SRC_DEVNAME);
    // Now printing the hardcoded address for clarity
    printf("  DMA Buffer       (UDMABuf): %s (Size: %zu KB, Phys Addr: 0x%lX)\n", 
           udma_buf_dev_name, dma_buffer_size / 1024, dma_phys_base);

    // --- Main Application Loop ---
    while(1) {
        display_menu();
        scanf(" %c", &choice);
        while(getchar() != '\n'); // Clear input buffer

        if (choice == '1') {
            run_axi_stream_source_test(dma_regs, stream_src_regs, dma_uio_fd, dma_phys_base, dma_virt_base);
        } else if (choice == '2') {
            run_diagnostics(dma_regs, stream_src_regs);
        } else if (choice == '3') { // Add case for new option
            run_axi_lite_reg_test(stream_src_regs);
        } else if (choice == 'Q' || choice == 'q') {
            break;
        } else {
            printf("Invalid option.\n");
        }
    }

    // --- Cleanup ---
    printf("\nCleaning up and exiting.\n");
    munmap((void*)dma_regs, 4096);
    munmap((void*)stream_src_regs, 4096);
    munmap(dma_virt_base, dma_buffer_size);
    close(dma_uio_fd);
    close(stream_src_uio_fd);
    close(udma_buf_fd);
    
    return 0;
}
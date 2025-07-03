#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <errno.h>
#include <stdint.h>
#include <time.h>      
#include "mpu_driver.h" 

typedef struct {
    volatile uint32_t CONFIG_REG;           
    volatile uint32_t BYTE_COUNT_REG;       
    volatile uint32_t SOURCE_ADDR_REG;      
    volatile uint32_t DEST_ADDR_REG;        
    volatile uint32_t NEXT_DESC_ADDR_REG;   
    uint8_t           _RESERVED[0x20 - 0x14]; 
} DmaDescriptorBlock_t;

typedef struct {
    volatile uint32_t CONFIG_REG;           
    volatile uint32_t BYTE_COUNT_REG;       
    volatile uint32_t DEST_ADDR_REG;        
} DmaStreamDescriptor_t;

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
} CoreAXI4DMAController_Regs_t;

#define UIO_DMA_DEVNAME         "dma-controller@60010000"

#define DDR_BUFFER_BASE         0xc8000000UL 

#define NUM_PING_PONG_BUFFERS   4
#define PING_PONG_BUFFER_SIZE   (1024*1024)
#define PING_PONG_SRC_BASE      DDR_BUFFER_BASE
#define PING_PONG_DEST_BASE     (DDR_BUFFER_BASE + (NUM_PING_PONG_BUFFERS * PING_PONG_BUFFER_SIZE))
#define STREAM_DEST_BASE        PING_PONG_DEST_BASE
#define STREAM_DESCRIPTOR_BASE  (STREAM_DEST_BASE + (NUM_PING_PONG_BUFFERS * PING_PONG_BUFFER_SIZE))
#define NUM_PING_PONG_TRANSFERS 16

#define MEM_FLAG_CHAIN          (1U << 10)
#define MEM_FLAG_IRQ_ON_PROCESS (1U << 12)
#define MEM_FLAG_SRC_RDY        (1U << 13)
#define MEM_FLAG_DEST_RDY       (1U << 14)
#define MEM_FLAG_VALID          (1U << 15)
#define MEM_OP_INCR             (0b01)
#define MEM_CONF_BASE           ((MEM_OP_INCR << 2) | MEM_OP_INCR | MEM_FLAG_CHAIN | MEM_FLAG_IRQ_ON_PROCESS)

#define STREAM_OP_INCR          (0b01)
#define STREAM_FLAG_DEST_RDY    (1U << 2)
#define STREAM_FLAG_VALID       (1U << 3)
#define STREAM_CONF_BASE        (STREAM_OP_INCR) 

#define FDMA_START(n)           (1U << (n))
#define FDMA_IRQ_MASK_ALL       (0x0FU)
#define FDMA_IRQ_CLEAR_ALL      (0x0FU)

#define SYSFS_PATH_LEN          (128)
#define ID_STR_LEN              (32)
#define UIO_DEVICE_PATH_LEN     (32)
#define NUM_UIO_DEVICES         (32)
#define MAP_SIZE                4096UL
#define PAGE_SIZE               sysconf(_SC_PAGE_SIZE)

static int get_uio_device_number(const char *id) {
    FILE *fp; int i; char file_id[ID_STR_LEN]; char sysfs_path[SYSFS_PATH_LEN];
    for (i = 0; i < NUM_UIO_DEVICES; i++) {
        snprintf(sysfs_path, SYSFS_PATH_LEN, "/sys/class/uio/uio%d/name", i);
        fp = fopen(sysfs_path, "r"); if (fp == NULL) break;
        fscanf(fp, "%31s", file_id); fclose(fp);
        if (strncmp(file_id, id, strlen(id)) == 0) return i;
    }
    return -1;
}

void force_dma_stop(CoreAXI4DMAController_Regs_t* dma_regs) {
    printf("  Forcing DMA stop by invalidating all internal and stream descriptors...\n");

    for (int i = 0; i < 32; ++i) {
        dma_regs->DESCRIPTOR[i].CONFIG_REG = 0;
    }

    for (int i = 0; i < 4; ++i) {
        dma_regs->STREAM_ADDR_REG[i] = 0;
    }
    __sync_synchronize(); 
}

void reset_interrupt_state(CoreAXI4DMAController_Regs_t* dma_regs, int dma_uio_fd) {
    printf("--- Resetting Interrupt State ---\n");

    force_dma_stop(dma_regs);

    dma_regs->INTR_0_MASK_REG = 0;
    dma_regs->INTR_0_CLEAR_REG = FDMA_IRQ_CLEAR_ALL;

    uint32_t status_after_clear = dma_regs->INTR_0_STAT_REG;
    if (status_after_clear != 0) {
        printf("  WARNING: Status register did not clear to 0 (0x%08X). This may indicate a persistent hardware state.\n", status_after_clear);
    } else {
        printf("  Status register successfully cleared.\n");
    }

    uint32_t dummy_irq_count;
    int flags = fcntl(dma_uio_fd, F_GETFL, 0);
    fcntl(dma_uio_fd, F_SETFL, flags | O_NONBLOCK);
    read(dma_uio_fd, &dummy_irq_count, sizeof(dummy_irq_count));
    fcntl(dma_uio_fd, F_SETFL, flags);

    uint32_t irq_enable = 1;
    write(dma_uio_fd, &irq_enable, sizeof(irq_enable));
    printf("--- Interrupt Reset Complete ---\n");
}

void generate_test_data(uint8_t* buffer, size_t size, uint8_t seed) {
    printf("  Generating %zu bytes of test data with seed 0x%02X...\n", size, seed);
    for (size_t i = 0; i < size; ++i) {
        buffer[i] = (uint8_t)((i + seed) * 13 + ((i + seed) >> 8) * 7);
    }
}

int verify_data_transfer(uint8_t* expected, uint8_t* actual, size_t size, int buffer_num) {
    printf("\n--- Verifying Buffer %d ---\n", buffer_num);
    size_t errors = 0;
    size_t first_error_offset = -1;

    for (size_t i = 0; i < size; ++i) {
        if (expected[i] != actual[i]) {
            if (errors == 0) {
                first_error_offset = i;
            }
            errors++;
        }
    }

    double percentage = 100.0 * (double)(size - errors) / (double)size;
    printf("  Verification Result: %.2f%% matched. %zu bytes transferred, %zu errors found.\n", percentage, size, errors);

    printf("  First 8 bytes received: ");
    for(int k=0; k<8; ++k) {
        printf("%02X ", actual[k]);
    }
    printf("\n");

    if (errors > 0) {
        printf("  ERROR: First mismatch at offset 0x%zX! Expected: 0x%02X, Got: 0x%02X\n", 
               first_error_offset, expected[first_error_offset], actual[first_error_offset]);
        return 0;
    } else {
        printf("  SUCCESS: Data integrity verified.\n");
        return 1;
    }
}

void run_mem_to_mem_ping_pong(CoreAXI4DMAController_Regs_t* dma_regs, int dma_uio_fd, int mem_fd) {
    printf("\n--- Running Memory-to-Memory Ping-Pong Test ---\n");

    reset_interrupt_state(dma_regs, dma_uio_fd);

    uint8_t *src_buf_map = mmap(NULL, NUM_PING_PONG_BUFFERS * PING_PONG_BUFFER_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, mem_fd, PING_PONG_SRC_BASE);
    if(src_buf_map == MAP_FAILED) { perror("Failed to mmap source buffers"); return; }

    for(int i = 0; i < NUM_PING_PONG_BUFFERS; ++i) {
        generate_test_data(src_buf_map + (i * PING_PONG_BUFFER_SIZE), PING_PONG_BUFFER_SIZE, i);
    }
    munmap(src_buf_map, NUM_PING_PONG_BUFFERS * PING_PONG_BUFFER_SIZE);

    printf("\n  Configuring %d descriptors for cyclic transfer...\n", NUM_PING_PONG_BUFFERS);
    for (int i = 0; i < NUM_PING_PONG_BUFFERS; ++i) {
        dma_regs->DESCRIPTOR[i].SOURCE_ADDR_REG = PING_PONG_SRC_BASE + (i * PING_PONG_BUFFER_SIZE);
        dma_regs->DESCRIPTOR[i].DEST_ADDR_REG   = PING_PONG_DEST_BASE + (i * PING_PONG_BUFFER_SIZE);
        dma_regs->DESCRIPTOR[i].BYTE_COUNT_REG  = PING_PONG_BUFFER_SIZE;
        dma_regs->DESCRIPTOR[i].NEXT_DESC_ADDR_REG = (i + 1) % NUM_PING_PONG_BUFFERS;
        dma_regs->DESCRIPTOR[i].CONFIG_REG = MEM_CONF_BASE | MEM_FLAG_SRC_RDY | MEM_FLAG_VALID;
    }

    dma_regs->INTR_0_MASK_REG = FDMA_IRQ_MASK_ALL;

    printf("  Starting ping-pong transfer for %d buffers...\n", NUM_PING_PONG_TRANSFERS);
    dma_regs->DESCRIPTOR[0].CONFIG_REG |= MEM_FLAG_DEST_RDY;
    dma_regs->START_OPERATION_REG = FDMA_START(0);

    for (int i = 0; i < NUM_PING_PONG_TRANSFERS; ++i) {
        uint32_t irq_count;
        read(dma_uio_fd, &irq_count, sizeof(irq_count));
        uint32_t status = dma_regs->INTR_0_STAT_REG;
        uint32_t completed_desc = (status >> 4) & 0x3F;
        printf("  Interrupt for Descriptor %u received.\n", completed_desc);

        if (i < (NUM_PING_PONG_TRANSFERS - 1)) {
            uint32_t next_desc_to_arm = (completed_desc + 1) % NUM_PING_PONG_BUFFERS;
            dma_regs->DESCRIPTOR[next_desc_to_arm].CONFIG_REG |= (MEM_FLAG_DEST_RDY | MEM_FLAG_SRC_RDY);
        } else {
            dma_regs->DESCRIPTOR[completed_desc].CONFIG_REG &= ~MEM_FLAG_CHAIN;
        }

        dma_regs->INTR_0_CLEAR_REG = FDMA_IRQ_CLEAR_ALL;

        uint32_t irq_enable = 1;
        write(dma_uio_fd, &irq_enable, sizeof(irq_enable));
    }

    dma_regs->INTR_0_MASK_REG = 0;
    force_dma_stop(dma_regs);

    printf("\n  All transfers complete. Verifying data integrity...\n");
    src_buf_map = mmap(NULL, NUM_PING_PONG_BUFFERS * PING_PONG_BUFFER_SIZE, PROT_READ, MAP_SHARED, mem_fd, PING_PONG_SRC_BASE);
    uint8_t *dest_buf_map = mmap(NULL, NUM_PING_PONG_BUFFERS * PING_PONG_BUFFER_SIZE, PROT_READ, MAP_SHARED, mem_fd, PING_PONG_DEST_BASE);

    if (src_buf_map == MAP_FAILED || dest_buf_map == MAP_FAILED) {
        perror("Failed to mmap buffers for verification");
    } else {
        int all_passed = 1;
        for (int i = 0; i < NUM_PING_PONG_BUFFERS; i++) {
            if (!verify_data_transfer(src_buf_map + (i * PING_PONG_BUFFER_SIZE), 
                                      dest_buf_map + (i * PING_PONG_BUFFER_SIZE), 
                                      PING_PONG_BUFFER_SIZE, i)) {
                all_passed = 0;
            }
        }
        if(all_passed) {
            printf("\n***** Mem-to-Mem Ping-Pong Test PASSED *****\n");
        } else {
            printf("\n***** Mem-to-Mem Ping-Pong Test FAILED *****\n");
        }
    }

    if (src_buf_map != MAP_FAILED) munmap(src_buf_map, NUM_PING_PONG_BUFFERS * PING_PONG_BUFFER_SIZE);
    if (dest_buf_map != MAP_FAILED) munmap(dest_buf_map, NUM_PING_PONG_BUFFERS * PING_PONG_BUFFER_SIZE);
}

void run_stream_ping_pong(CoreAXI4DMAController_Regs_t* dma_regs, int dma_uio_fd, int mem_fd) {
    printf("\n--- Running AXI4-Stream to Memory Ping-Pong Test ---\n");

    reset_interrupt_state(dma_regs, dma_uio_fd);

    DmaStreamDescriptor_t *desc_region_vaddr = NULL;
    uint8_t *dest_bufs[NUM_PING_PONG_BUFFERS];
    uint8_t* expected_data = malloc(PING_PONG_BUFFER_SIZE);
    if (!expected_data) {
        perror("Failed to allocate verification buffer");
        return;
    }

    size_t desc_region_size = NUM_PING_PONG_BUFFERS * sizeof(DmaStreamDescriptor_t);
    desc_region_vaddr = mmap(NULL, desc_region_size, PROT_READ | PROT_WRITE, MAP_SHARED, mem_fd, STREAM_DESCRIPTOR_BASE);
    if (desc_region_vaddr == MAP_FAILED) {
        perror("Failed to mmap stream descriptor region");
        free(expected_data);
        return;
    }

    printf("  Mapping %d destination buffers and preparing descriptors in DDR...\n", NUM_PING_PONG_BUFFERS);
    for (int i = 0; i < NUM_PING_PONG_BUFFERS; ++i) {
        dest_bufs[i] = mmap(NULL, PING_PONG_BUFFER_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, mem_fd, STREAM_DEST_BASE + (i * PING_PONG_BUFFER_SIZE));
        if (dest_bufs[i] == MAP_FAILED) { 
            perror("Failed to mmap stream destination buffer"); 
            munmap(desc_region_vaddr, desc_region_size); 
            free(expected_data);
            return; 
        }
        memset(dest_bufs[i], 0, PING_PONG_BUFFER_SIZE);

        DmaStreamDescriptor_t* current_desc = desc_region_vaddr + i;
        current_desc->DEST_ADDR_REG = STREAM_DEST_BASE + (i * PING_PONG_BUFFER_SIZE);
        current_desc->BYTE_COUNT_REG = PING_PONG_BUFFER_SIZE;
        current_desc->CONFIG_REG = STREAM_CONF_BASE | STREAM_FLAG_VALID | STREAM_FLAG_DEST_RDY;
    }
    __sync_synchronize();

    printf("\n*****************************************************************\n");
    printf("  System configured for continuous stream reception.\n");
    printf("  Please initiate the AXI4-Stream transfer from your hardware now.\n");
    printf("  The stream source should send a unique pattern for each packet.\n");
    printf("*****************************************************************\n\n");

    dma_regs->INTR_0_MASK_REG = FDMA_IRQ_MASK_ALL;
    int next_dma_desc_idx = 0;
    int transfers_completed = 0;

    dma_regs->STREAM_ADDR_REG[0] = STREAM_DESCRIPTOR_BASE + (next_dma_desc_idx * sizeof(DmaStreamDescriptor_t));

    while(transfers_completed < NUM_PING_PONG_TRANSFERS) {
        uint32_t irq_count;
        printf("Waiting for interrupt for buffer %d...\n", next_dma_desc_idx);

        read(dma_uio_fd, &irq_count, sizeof(irq_count));

        uint32_t status = dma_regs->INTR_0_STAT_REG;

        uint32_t desc_num = (status >> 4) & 0x3F;
        int ops_complete = status & 0x1;

        if (ops_complete && desc_num == 33) {
            int completed_idx = next_dma_desc_idx;

            generate_test_data(expected_data, PING_PONG_BUFFER_SIZE, completed_idx);
            verify_data_transfer(expected_data, dest_bufs[completed_idx], PING_PONG_BUFFER_SIZE, completed_idx);

            next_dma_desc_idx = (next_dma_desc_idx + 1) % NUM_PING_PONG_BUFFERS;

            dma_regs->STREAM_ADDR_REG[0] = STREAM_DESCRIPTOR_BASE + (next_dma_desc_idx * sizeof(DmaStreamDescriptor_t));

            transfers_completed++;
        } else {
            printf("  ERROR: Received an unexpected or error interrupt (Status: 0x%08X). Halting test.\n", status);
            break;
        }

        dma_regs->INTR_0_CLEAR_REG = FDMA_IRQ_CLEAR_ALL;

        uint32_t irq_enable = 1;
        write(dma_uio_fd, &irq_enable, sizeof(irq_enable));
    }

    dma_regs->INTR_0_MASK_REG = 0;
    printf("\n***** AXI-Stream Ping-Pong Test Complete *****\n");

    free(expected_data);
    munmap(desc_region_vaddr, desc_region_size);
    for (int i = 0; i < NUM_PING_PONG_BUFFERS; ++i) {
        munmap(dest_bufs[i], PING_PONG_BUFFER_SIZE);
    }
}

int main(void) {
    int dma_uio_fd = -1, mem_fd = -1, uio_num;
    CoreAXI4DMAController_Regs_t *dma_regs = NULL;
    char cmd;

    printf("--- PolarFire SoC DMA Test Application ---\n");

    if (!MPU_Configure_FIC0()) {
        fprintf(stderr, "Fatal: Could not configure MPU. Halting.\n");
        return 1;
    }

    uio_num = get_uio_device_number(UIO_DMA_DEVNAME);
    if (uio_num < 0) {
        fprintf(stderr, "Fatal: Could not find UIO device for %s.\n", UIO_DMA_DEVNAME);
        return 1;
    }

    char uio_dev_path[UIO_DEVICE_PATH_LEN];
    snprintf(uio_dev_path, UIO_DEVICE_PATH_LEN, "/dev/uio%d", uio_num);
    dma_uio_fd = open(uio_dev_path, O_RDWR);
    if (dma_uio_fd < 0) { perror("Fatal: Failed to open UIO device file"); return 1; }

    dma_regs = mmap(NULL, MAP_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, dma_uio_fd, 0);
    if (dma_regs == MAP_FAILED) { perror("Fatal: Failed to mmap UIO device"); close(dma_uio_fd); return 1; }

    mem_fd = open("/dev/mem", O_RDWR | O_SYNC);
    if (mem_fd < 0) { perror("Fatal: Failed to open /dev/mem"); munmap(dma_regs, MAP_SIZE); close(dma_uio_fd); return 1; }

    printf("Reading DMA Controller Version: 0x%08X\n", dma_regs->VERSION_REG);

    while(1){
        printf("\n# Choose one of the following options:\n");
        printf("  1 - Run Memory-to-Memory Ping-Pong Test\n");
        printf("  2 - Run AXI4-Stream to Memory Ping-Pong Test\n");
        printf("  3 - Exit\n> ");

        scanf(" %c", &cmd);
        while(getchar() != '\n'); 

        if (cmd == '1') {
            run_mem_to_mem_ping_pong(dma_regs, dma_uio_fd, mem_fd);
        } else if (cmd == '2') {
            run_stream_ping_pong(dma_regs, dma_uio_fd, mem_fd);
        } else if (cmd == '3' || cmd == 'q') {
            break;
        } else {
            printf("Invalid option.\n");
        }
    }

    munmap(dma_regs, MAP_SIZE);
    close(dma_uio_fd);
    close(mem_fd);
    printf("\nExiting.\n");

    return 0;
}
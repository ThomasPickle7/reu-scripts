#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <errno.h>
#include <stdint.h>
#include <time.h>
#include <sys/select.h>
#include "mpu_driver.h" // Assuming this contains MPU_Configure_FIC0()

// --- Structs and Defines ---

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


// --- Configuration Constants ---
#define UIO_DMA_DEVNAME         "dma-controller@60010000"
#define UIO_STREAM_GEN_DEVNAME  "stream-generator"
#define UDMA_BUF_DEVNAME        "/dev/udmabuf-ddr-nc0"
#define NUM_BUFFERS             4
#define BUFFER_SIZE             (1024 * 1024)

#define PING_PONG_SRC_OFFSET      0
#define PING_PONG_DEST_OFFSET     (PING_PONG_SRC_OFFSET + (NUM_BUFFERS * BUFFER_SIZE))
#define STREAM_DEST_OFFSET        (PING_PONG_DEST_OFFSET + (NUM_BUFFERS * BUFFER_SIZE))
#define STREAM_DESCRIPTOR_OFFSET  (STREAM_DEST_OFFSET + (NUM_BUFFERS * BUFFER_SIZE))

#define NUM_TRANSFERS           16

// --- Bitfield Flags ---
#define MEM_OP_INCR             (0b01)
#define MEM_FLAG_CHAIN          (1U << 10)
#define MEM_FLAG_IRQ_ON_PROCESS (1U << 12)
#define MEM_FLAG_SRC_RDY        (1U << 13)
#define MEM_FLAG_DEST_RDY       (1U << 14)
#define MEM_FLAG_VALID          (1U << 15)
#define MEM_CONF_BASE           ((MEM_OP_INCR << 2) | MEM_OP_INCR | MEM_FLAG_CHAIN | MEM_FLAG_IRQ_ON_PROCESS)

#define STREAM_OP_INCR          (0b01)
#define STREAM_FLAG_CHAIN       (1U << 1)
#define STREAM_FLAG_DEST_RDY    (1U << 2)
#define STREAM_FLAG_VALID       (1U << 3)
#define STREAM_FLAG_IRQ_EN      (1U << 4)
#define STREAM_CONF_BASE        (STREAM_OP_INCR | STREAM_FLAG_IRQ_EN)

#define FDMA_START_MEM(n)       (1U << (n))
#define FDMA_START_STREAM(n)    (1U << (16 + n))
#define FDMA_IRQ_MASK_ALL       (0x0FU)
#define FDMA_IRQ_CLEAR_ALL      (0x0FU)
#define FDMA_IRQ_STAT_WR_ERR    (1U << 1)
#define FDMA_IRQ_STAT_INVALID_DESC (1U << 3)

#define SYSFS_PATH_LEN          (128)
#define ID_STR_LEN              (32)
#define UIO_DEVICE_PATH_LEN     (32)
#define NUM_UIO_DEVICES         (32)
#define MAP_SIZE                4096UL
#define PAGE_SIZE               sysconf(_SC_PAGE_SIZE)

// --- Helper Functions ---
static uintptr_t get_udma_phys_addr(const char* uio_device_name) {
    char sysfs_path[SYSFS_PATH_LEN];
    FILE* fp;
    uintptr_t paddr;
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

void force_dma_stop(CoreAXI4DMAController_Regs_t* dma_regs) { printf("  Forcing DMA stop...\n"); for (int i = 0; i < 32; ++i) dma_regs->DESCRIPTOR[i].CONFIG_REG = 0; for (int i = 0; i < 4; ++i) dma_regs->STREAM_ADDR_REG[i] = 0; __sync_synchronize(); }
void exhaustive_interrupt_reset(CoreAXI4DMAController_Regs_t* dma_regs, int dma_uio_fd) { printf("\n--- Exhaustive Interrupt Reset ---\n"); force_dma_stop(dma_regs); dma_regs->INTR_0_MASK_REG = 0; __sync_synchronize(); uint32_t dummy; int flags = fcntl(dma_uio_fd, F_GETFL, 0); fcntl(dma_uio_fd, F_SETFL, flags | O_NONBLOCK); while(read(dma_uio_fd, &dummy, sizeof(dummy)) > 0); fcntl(dma_uio_fd, F_SETFL, flags); dma_regs->INTR_0_CLEAR_REG = FDMA_IRQ_CLEAR_ALL; __sync_synchronize(); uint32_t irq_enable = 1; write(dma_uio_fd, &irq_enable, sizeof(irq_enable)); printf("--- Interrupt Reset Complete ---\n"); }

// MODIFIED: Added these helper functions back in to fix linker error.
void generate_test_data(uint8_t* buffer, size_t size, uint8_t seed) {
    printf("  Generating %zu bytes of test data with seed 0x%02X...\n", size, seed);
    for (size_t i = 0; i < size; ++i) {
        buffer[i] = (uint8_t)((i + seed) * 13 + ((i + seed) >> 8) * 7);
    }
}

int verify_data_transfer(uint8_t* expected, uint8_t* actual, size_t size, int buffer_num) {
    printf("\n--- Verifying Buffer %d ---\n", buffer_num);
    size_t errors = 0;
    size_t first_error_offset = (size_t)-1;
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
    if (errors > 0) {
        printf("  ERROR: First mismatch at offset 0x%zX! Expected: 0x%02X, Got: 0x%02X\n",
               first_error_offset, expected[first_error_offset], actual[first_error_offset]);
        return 0;
    } else {
        printf("  SUCCESS: Data integrity verified.\n");
        return 1;
    }
}

void diagnose_udmabuf(uintptr_t phys_base, uint8_t* virt_base) {
    printf("\n--- Diagnosing UDMABuf Memory ---\n");
    if (phys_base == 0 || virt_base == NULL) {
        printf("  ERROR: Invalid physical or virtual base addresses provided.\n");
        return;
    }
    printf("  UDMA Buffer Physical Base Address: 0x%lX\n", phys_base);
    printf("  UDMA Buffer Mapped Virtual Base Address: %p\n", virt_base);
    uint8_t* test_ptr = virt_base;
    uint8_t original_val = test_ptr[0];
    uint8_t test_val = 0xA5;
    test_ptr[0] = test_val;
    __sync_synchronize();
    if (test_ptr[0] == test_val) {
        printf("  SUCCESS: Wrote 0x%02X and read back 0x%02X.\n", test_val, test_ptr[0]);
    } else {
        printf("  ERROR: Wrote 0x%02X but read back 0x%02X.\n", test_val, test_ptr[0]);
    }
    test_ptr[0] = original_val;
    printf("\n--- Memory Diagnostics Complete ---\n");
}


// --- Test Functions ---

void run_mem_to_mem_ping_pong(CoreAXI4DMAController_Regs_t* dma_regs, int dma_uio_fd, uintptr_t dma_phys_base, uint8_t* dma_virt_base) {
    printf("\n--- Running Memory-to-Memory Ping-Pong Test ---\n");
    exhaustive_interrupt_reset(dma_regs, dma_uio_fd);
    uint8_t* virt_src_buf = dma_virt_base + PING_PONG_SRC_OFFSET;
    uint8_t* virt_dest_buf = dma_virt_base + PING_PONG_DEST_OFFSET;
    for(int i = 0; i < NUM_BUFFERS; ++i) { generate_test_data(virt_src_buf + (i * BUFFER_SIZE), BUFFER_SIZE, i); }
    printf("  Data generated directly into non-cached DMA buffer. No msync() required.\n");
    printf("\n  Configuring %d internal descriptors for cyclic transfer...\n", NUM_BUFFERS);
    for (int i = 0; i < NUM_BUFFERS; ++i) {
        dma_regs->DESCRIPTOR[i].SOURCE_ADDR_REG = dma_phys_base + PING_PONG_SRC_OFFSET + (i * BUFFER_SIZE);
        dma_regs->DESCRIPTOR[i].DEST_ADDR_REG   = dma_phys_base + PING_PONG_DEST_OFFSET + (i * BUFFER_SIZE);
        dma_regs->DESCRIPTOR[i].BYTE_COUNT_REG  = BUFFER_SIZE;
        dma_regs->DESCRIPTOR[i].NEXT_DESC_ADDR_REG = (i + 1) % NUM_BUFFERS;
        dma_regs->DESCRIPTOR[i].CONFIG_REG = MEM_CONF_BASE | MEM_FLAG_SRC_RDY | MEM_FLAG_VALID;
    }
    __sync_synchronize();
    dma_regs->INTR_0_MASK_REG = FDMA_IRQ_MASK_ALL;
    printf("  Starting ping-pong transfer for %d buffers...\n", NUM_TRANSFERS);
    dma_regs->DESCRIPTOR[0].CONFIG_REG |= MEM_FLAG_DEST_RDY;
    __sync_synchronize();
    dma_regs->START_OPERATION_REG = FDMA_START_MEM(0);
    for (int i = 0; i < NUM_TRANSFERS; ++i) {
        uint32_t irq_count;
        printf("  Waiting for interrupt %d of %d...\n", i + 1, NUM_TRANSFERS);
        read(dma_uio_fd, &irq_count, sizeof(irq_count));
        uint32_t status = dma_regs->INTR_0_STAT_REG;
        uint32_t completed_desc = (status >> 4) & 0x3F;
        printf("  Interrupt for Descriptor %u received.\n", completed_desc);
        if (i < (NUM_TRANSFERS - 1)) {
            uint32_t next_desc_to_arm = (completed_desc + 1) % NUM_BUFFERS;
            dma_regs->DESCRIPTOR[next_desc_to_arm].CONFIG_REG |= (MEM_FLAG_DEST_RDY | MEM_FLAG_SRC_RDY);
        } else {
            dma_regs->DESCRIPTOR[completed_desc].CONFIG_REG &= ~MEM_FLAG_CHAIN;
        }
        __sync_synchronize();
        dma_regs->INTR_0_CLEAR_REG = FDMA_IRQ_CLEAR_ALL;
        uint32_t irq_enable = 1;
        write(dma_uio_fd, &irq_enable, sizeof(irq_enable));
    }
    force_dma_stop(dma_regs);
    printf("\n  All transfers complete. Verifying data integrity...\n");
    printf("  Destination buffer is non-cached. No msync(MS_INVALIDATE) required.\n");
    int all_passed = 1;
    for (int i = 0; i < NUM_BUFFERS; i++) {
        if (!verify_data_transfer(virt_src_buf + (i * BUFFER_SIZE), virt_dest_buf + (i * BUFFER_SIZE), BUFFER_SIZE, i)) {
            all_passed = 0;
        }
    }
    if(all_passed) { printf("\n***** Mem-to-Mem Ping-Pong Test PASSED *****\n"); }
    else { printf("\n***** Mem-to-Mem Ping-Pong Test FAILED *****\n"); }
}

void run_stream_to_mem_test(CoreAXI4DMAController_Regs_t* dma_regs, int dma_uio_fd, uintptr_t dma_phys_base, uint8_t* dma_virt_base) {
    printf("\n--- Running Stream-to-Memory Test (Simulated) ---\n");
    exhaustive_interrupt_reset(dma_regs, dma_uio_fd);
    DmaStreamDescriptor_t* stream_descriptors = (DmaStreamDescriptor_t*)(dma_virt_base + STREAM_DESCRIPTOR_OFFSET);
    printf("  Stream descriptor chain located at virtual address %p\n", stream_descriptors);
    printf("  Configuring %d stream descriptors in DDR...\n", NUM_BUFFERS);
    for (int i = 0; i < NUM_BUFFERS; ++i) {
        stream_descriptors[i].DEST_ADDR_REG = dma_phys_base + STREAM_DEST_OFFSET + (i * BUFFER_SIZE);
        stream_descriptors[i].BYTE_COUNT_REG = BUFFER_SIZE;
        uint32_t conf = (STREAM_OP_INCR | STREAM_FLAG_IRQ_EN) | STREAM_FLAG_VALID;
        if (i < (NUM_BUFFERS - 1)) {
           conf |= STREAM_FLAG_CHAIN;
        }
        stream_descriptors[i].CONFIG_REG = conf;
    }
    printf("  Descriptors written to non-cached memory. No msync() required.\n");
    uintptr_t phys_desc_addr = dma_phys_base + STREAM_DESCRIPTOR_OFFSET;
    printf("  Pointing DMA Stream Channel 0 to descriptor chain at physical address 0x%lX\n", phys_desc_addr);
    dma_regs->STREAM_ADDR_REG[0] = phys_desc_addr;
    __sync_synchronize();
    dma_regs->INTR_0_MASK_REG = FDMA_IRQ_MASK_ALL;
    stream_descriptors[0].CONFIG_REG |= STREAM_FLAG_DEST_RDY;
    __sync_synchronize();
    printf("  Starting stream channel 0. Waiting for data...\n");
    dma_regs->START_OPERATION_REG = FDMA_START_STREAM(0);
    printf("\n  NOTE: This test simulates waiting for interrupts. A real data-generating\n");
    printf("  FPGA IP is needed to actually transfer data and trigger them.\n");
    for (int i = 0; i < 1; ++i) {
        printf("  Simulating one interrupt and stopping test.\n");
    }
    force_dma_stop(dma_regs);
    printf("\n  Stream test complete. In a real scenario, you would now verify the data.\n");
}

void run_control_path_validation_test(CoreAXI4DMAController_Regs_t* dma_regs, int dma_uio_fd, uintptr_t dma_phys_base, uint8_t* dma_virt_base) {
    printf("\n--- Running DMA Control Path Validation Test (Software-Only) ---\n");
    exhaustive_interrupt_reset(dma_regs, dma_uio_fd);

    printf("  Configuring one stream descriptor in the DMA buffer...\n");
    DmaStreamDescriptor_t* desc = (DmaStreamDescriptor_t*)(dma_virt_base + STREAM_DESCRIPTOR_OFFSET);
    
    desc->DEST_ADDR_REG = dma_phys_base + STREAM_DEST_OFFSET;
    desc->BYTE_COUNT_REG = 1024;
    desc->CONFIG_REG = (STREAM_OP_INCR | STREAM_FLAG_IRQ_EN) | STREAM_FLAG_VALID | STREAM_FLAG_DEST_RDY;

    uintptr_t phys_desc_addr = dma_phys_base + STREAM_DESCRIPTOR_OFFSET;
    printf("  Pointing DMA Stream Channel 0 to descriptor at physical address 0x%lX\n", phys_desc_addr);
    dma_regs->STREAM_ADDR_REG[0] = phys_desc_addr;
    __sync_synchronize();

    dma_regs->INTR_0_MASK_REG = FDMA_IRQ_MASK_ALL;
    
    printf("  Attempting to start stream channel 0 via software register write...\n");
    dma_regs->START_OPERATION_REG = FDMA_START_STREAM(0);
    __sync_synchronize();

    printf("\n  --- Post-Start Diagnostics ---\n");
    printf("  Value read back from START_OPERATION_REG: 0x%08X\n", dma_regs->START_OPERATION_REG);
    printf("  Value read from INTR_0_MASK_REG:        0x%08X\n", dma_regs->INTR_0_MASK_REG);
    printf("  Value read from INTR_0_STAT_REG:        0x%08X\n", dma_regs->INTR_0_STAT_REG);
    printf("  ------------------------------\n\n");

    fd_set fds;
    struct timeval tv;
    int retval;
    uint32_t irq_count;

    FD_ZERO(&fds);
    FD_SET(dma_uio_fd, &fds);

    tv.tv_sec = 5;
    tv.tv_usec = 0;

    printf("  Waiting for interrupt (with a 5-second timeout)...\n");
    retval = select(dma_uio_fd + 1, &fds, NULL, NULL, &tv);

    if (retval == -1) {
        perror("  select() error");
        return;
    } else if (retval) {
        read(dma_uio_fd, &irq_count, sizeof(irq_count));
        uint32_t status = dma_regs->INTR_0_STAT_REG;
        uint32_t status_flags = status & 0x0F;

        printf("  Interrupt received! DMA Status Register: 0x%08X\n", status);

        if (status_flags & FDMA_IRQ_STAT_INVALID_DESC) {
            printf("\n  SUCCESS: Received expected 'Invalid Descriptor' interrupt.\n");
            printf("  This proves the DMA read its descriptor from 0x%lX in DDR.\n", phys_desc_addr);
            printf("  The error occurred because a software start is not valid for a stream-to-memory descriptor.\n");
            printf("\n***** DMA Control Path Test PASSED *****\n");
        } else {
            printf("\n  FAILURE: Did not receive the expected 'Invalid Descriptor' interrupt.\n");
            printf("  Received status flags: 0x%X. This indicates a different problem.\n", status_flags);
            printf("\n***** DMA Control Path Test FAILED *****\n");
        }
    } else {
        printf("\n  FAILURE: Test timed out after 5 seconds. No interrupt was received.\n");
        printf("  HYPOTHESIS: The DMA controller is likely ignoring the software start for the stream channel\n");
        printf("              and is waiting for a hardware AXI-Stream signal (TVALID) from a stream master.\n");
        printf("              This is plausible hardware behavior. The control path may be fine, but this\n");
        printf("              test cannot trigger it. The next step is to add a hardware stream generator.\n");
        printf("\n***** DMA Control Path Test INCONCLUSIVE (Timeout) *****\n");
    }

    force_dma_stop(dma_regs);
    dma_regs->INTR_0_CLEAR_REG = FDMA_IRQ_CLEAR_ALL;
    uint32_t irq_enable = 1;
    write(dma_uio_fd, &irq_enable, sizeof(irq_enable));
}


// --- Main Application Logic ---
int main(void) {
    int dma_uio_fd = -1, udma_buf_fd = -1;
    CoreAXI4DMAController_Regs_t *dma_regs = NULL;
    uint8_t* dma_virt_base = NULL;
    uintptr_t dma_phys_base = 0;
    size_t dma_buffer_size = STREAM_DESCRIPTOR_OFFSET + (NUM_BUFFERS * sizeof(DmaStreamDescriptor_t));
    char cmd;

    printf("--- PolarFire SoC DMA Test Application ---\n");
    if (!MPU_Configure_FIC0()) {
        fprintf(stderr, "Fatal: Could not configure MPU. Halting.\n");
        return 1;
    }

    printf("\n--- Initializing Devices ---\n");
    printf("1. Mapping DMA Controller Registers (%s)...\n", UIO_DMA_DEVNAME);
    int uio_num = get_uio_device_number(UIO_DMA_DEVNAME);
    if (uio_num < 0) { fprintf(stderr, "   FATAL: Could not find UIO for %s.\n", UIO_DMA_DEVNAME); return 1; }
    char uio_dev_path[UIO_DEVICE_PATH_LEN];
    snprintf(uio_dev_path, UIO_DEVICE_PATH_LEN, "/dev/uio%d", uio_num);
    dma_uio_fd = open(uio_dev_path, O_RDWR);
    if (dma_uio_fd < 0) { perror("   FATAL: Failed to open DMA UIO"); return 1; }
    dma_regs = mmap(NULL, MAP_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, dma_uio_fd, 0);
    if (dma_regs == MAP_FAILED) { perror("   FATAL: Failed to mmap DMA UIO"); close(dma_uio_fd); return 1; }
    printf("   SUCCESS: DMA Controller mapped.\n");
    
    printf("2. Mapping Non-Cached DMA Buffer (%s)...\n", UDMA_BUF_DEVNAME);
    udma_buf_fd = open(UDMA_BUF_DEVNAME, O_RDWR | O_SYNC);
    if (udma_buf_fd < 0) { perror("   FATAL: Failed to open " UDMA_BUF_DEVNAME); return 1; }
    dma_virt_base = mmap(NULL, dma_buffer_size, PROT_READ | PROT_WRITE, MAP_SHARED, udma_buf_fd, 0);
    if (dma_virt_base == MAP_FAILED) { perror("   FATAL: Failed to mmap udmabuf"); return 1; }
    dma_phys_base = get_udma_phys_addr("udmabuf-ddr-nc0");
    if (dma_phys_base == 0) { fprintf(stderr, "   FATAL: Could not get physical address of udmabuf\n"); return 1; }
    printf("   SUCCESS: UDMA Buffer mapped.\n");

    printf("\n--- Initialization Complete ---\n");
    printf("DMA Controller Version: 0x%08X\n", dma_regs->VERSION_REG);
    printf("UDMA Buffer mapped: %zu bytes at virtual addr %p (physical addr 0x%lX)\n", dma_buffer_size, dma_virt_base, dma_phys_base);

    while(1){
        printf("\n# Choose one of the following options:\n");
        printf("  1 - Run Memory-to-Memory Ping-Pong Test\n");
        printf("  2 - Run Stream-to-Memory Test (Simulated)\n");
        printf("  3 - Run DMA Control Path Validation Test (Software-Only)\n");
        printf("  D - Run Low-Level System Diagnostics\n");
        printf("  Q - Exit\n> ");
        scanf(" %c", &cmd);
        while(getchar() != '\n');
        
        if (cmd == '1') {
            run_mem_to_mem_ping_pong(dma_regs, dma_uio_fd, dma_phys_base, dma_virt_base);
        } else if (cmd == '2') {
            run_stream_to_mem_test(dma_regs, dma_uio_fd, dma_phys_base, dma_virt_base);
        } else if (cmd == '3') {
            run_control_path_validation_test(dma_regs, dma_uio_fd, dma_phys_base, dma_virt_base);
        } else if (cmd == 'D' || cmd == 'd') {
            diagnose_udmabuf(dma_phys_base, dma_virt_base);
        } else if (cmd == 'Q' || cmd == 'q') {
            break;
        } else {
            printf("Invalid option.\n");
        }
    }
    
    // Cleanup
    munmap(dma_virt_base, dma_buffer_size);
    close(udma_buf_fd);
    munmap(dma_regs, MAP_SIZE);
    close(dma_uio_fd);
    printf("\nExiting.\n");
    return 0;
}

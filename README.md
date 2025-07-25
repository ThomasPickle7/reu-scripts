# RadioHound High-Speed Data Acquisition System

## 1\. Overview

This repository contains the C source code, reference materials, and development environment configurations for the RadioHound High-Speed Data Acquisition project. The goal of this project is to implement a high-throughput data path from an external ADC into the BeagleV-Fire's DDR memory using its FPGA.

This README provides a guide to the structure and contents of this repository. For a detailed explanation of the system architecture, project status, and development methodology, please refer to the **"RadioHound Transfer Documents"**.

## 2\. Repository Structure

The repository is organized into several key directories:

```
.
├── .vscode/
├── driver_code/
├── ref/
└── software/
    ├── app/
    ├── bsp/
    └── drivers/
```

### `/software/` - Primary Application Code

This directory contains the main, actively developed source code for the project. It is intended to be compiled and run on the BeagleV-Fire's RISC-V processor.

  * **`/app/`**: Contains the high-level application logic.

      * `main.c`: The main entry point for the test application. It initializes the necessary drivers and orchestrates the DMA tests.
      * `stream_tests.c`, `stream_tests.h`: Implements the core DMA test functions, including the working memory-to-memory test and the currently-hanging stream-to-memory test.
      * `radiohound_dma_api.c`, `radiohound_dma_api.h`: A high-level API that abstracts the details of DMA setup and control, providing simpler functions for application use.
      * `diagnostics.c`, `diagnostics.h`: Provides helper functions for printing formatted output and diagnostic information to the serial console.

  * **`/bsp/` (Board Support Package)**: Contains files specific to the hardware platform.

      * `hw_platform.h`: Defines hardware-specific constants, such as the base addresses for peripherals like the DMA controller and the custom stream packer. These addresses are derived from the Libero SoC memory map.

  * **`/drivers/`**: Contains the low-level drivers for controlling the FPGA peripherals.

      * `dma_driver.c`, `dma_driver.h`: The driver for the Microchip `CoreAXI4DMAController`. It handles the initialization of the DMA engine and the setup of DMA descriptors.
      * `mpu_driver.c`, `mpu_driver.h`: A critical driver for configuring the RISC-V **Memory Protection Unit (MPU)**. Its sole purpose is to grant the DMA controller master access to the non-cached DDR memory region, which is essential for any DMA operation to succeed.

### `/ref/` - Reference Materials

This directory is a collection of essential datasheets, hardware configuration files, and reference code.

  * **Datasheets (PDF):**

      * `CoreAXI4DMAController_HB.pdf`: The official handbook for the DMA controller IP.
      * `CoreAXI4Interconnect_HB.pdf`: The handbook for the AXI bus interconnect IP.
      * `4PolarFire_SoC_FPGA_MSS_Technical_Reference_Manual_VC.pdf`: The comprehensive technical manual for the PolarFire SoC, which is invaluable for understanding the MPU and memory subsystem.

  * **FPGA Configuration (JSON):** These files are generated by the Libero SoC software and define the hardware layout of the custom FPGA design.

      * `MY_CUSTOM_FPGA_DESIGN_64FE066A_memory_map.json`: Defines the base addresses for all memory-mapped peripherals in the FPGA fabric.
      * `MY_CUSTOM_FPGA_DESIGN_64FE066A_interrupt_map.json`: Defines the interrupt connections for the FPGA design.

### `/driver_code/` - Legacy/Archived Code

This directory contains an older version of the driver and application code. While it may be useful for historical context, the code in the `/software/` directory is the most current and should be considered the primary source for future development.

## 3\. How to Use This Repository


Once the FPGA bitstream has been transferred and flashed on the BeagleV-Fire:
      * Transfer the `/software` directory to the BeagleV-Fire via `scp`.
      * Navigate to the directory on the board and run `make` to compile the C code.
      * Execute the test application with `sudo ./build/radiohound`.

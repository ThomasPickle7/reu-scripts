# RadioHound High-Speed Data Acquisition System

This document provides a comprehensive guide to the design, status, and development process for the RadioHound high-speed data acquisition system. The project's primary goal is to stream ADC data from a RadioHound sensor through the FPGA fabric of a BeagleV-Fire board and into its DDR memory at the highest possible throughput. [cite\_start]This serves as the foundational infrastructure to significantly increase the RadioHound project's sampling rate beyond its current 48MHz capability [cite: 16355].

## System Architecture

The system is architected to create a direct, high-throughput data path from the external ADC to the main DDR memory, minimizing CPU overhead. [cite\_start]This is achieved by leveraging the FPGA for data packing and transfer, with the CPU's role limited to configuration and control[cite: 16365].

[cite\_start]The architecture is centered around Microchip's `CoreAXI4DMAController` IP, which is capable of achieving transfer rates up to 7488 Mb/s[cite: 16356].

### Core Components

#### Hardware (FPGA Fabric)

  * [cite\_start]**I/O (ADC):** An external 8-bit ADC from the RadioHound sensor board provides the raw data samples[cite: 16367].
  * **Stream Packer (`axi_stream_source.v`):** A custom Verilog module that aggregates eight 8-bit samples into a single 64-bit word. [cite\_start]This packing is essential to maximize the efficiency of the 64-bit AXI4 bus[cite: 16369, 16370]. [cite\_start]It is controlled via memory-mapped registers at base address `0x60000000`[cite: 16371].
  * **DMA Interface (`CoreAXI4DMAController`):** This Microchip IP core manages the data movement. [cite\_start]It reads data from the AXI4-Stream source (the packer) and writes it to the DDR memory[cite: 16374]. [cite\_start]Its control registers are located at base address `0x60010000`[cite: 16375].
  * [cite\_start]**AXI Interconnect (`CoreAXI4Interconnect`):** This IP core acts as the central bus matrix, connecting all FPGA peripherals to the CPU's memory subsystem and defining the memory map for the FPGA design[cite: 16376, 16377].

#### Software & Memory (CPU Stack)

  * **DDR Memory:** The main system memory. [cite\_start]DMA operations specifically target a **non-cached** memory region starting at `0xC0000000` to prevent CPU cache coherency issues and maximize throughput[cite: 16379, 16380].
  * **C Drivers:** A suite of bare-metal C files that configure and control the hardware components.
      * **`mpu_driver.c`:** This critical driver configures the **Memory Protection Unit (MPU)**. It grants the DMA controller (an AXI master) the necessary permissions to read and write to the non-cached DDR memory space. [cite\_start]Without this configuration, all DMA transfers would fail[cite: 16382, 16383, 16384].
      * [cite\_start]**`dma_driver.c`:** This driver provides the API to initialize, configure, and manage the `CoreAXI4DMAController`, primarily by setting up DMA descriptors in memory[cite: 16385, 16386, 16387].

## Project Status

### Completed & Verified

  * [cite\_start]**MPU Driver (`mpu_driver.c`):** Correctly configures the MPU to grant the FPGA fabric access to the non-cached DDR memory range `0xC0000000` - `0xDFFFFFFF`[cite: 16390].
  * [cite\_start]**DMA Driver (`dma_driver.c`):** Core functions for DMA initialization and descriptor setup are implemented and stable[cite: 16391].
  * **Memory-to-Memory DMA Test:** The `run_mem_to_mem_ping_pong` test in `main.c` passes successfully. [cite\_start]This test verifies that the MPU is configured correctly, the CPU can communicate with the DMA, and the DMA can read from and write to DDR memory[cite: 16392, 16393, 16394, 16395, 16396].

### In Progress

  * **Stream-to-Memory DMA Test:** The software for this test (`run_stream_to_mem_test`) is implemented and correctly configures the DMA to receive a stream. However, the test currently **hangs** because the DMA controller is waiting for the `TVALID` AXI-Stream signal from a hardware source. This signal is never asserted because the `axi_stream_source.v` packer is not yet connected to the DMA controller in the FPGA design. [cite\_start]This is the project's primary bottleneck[cite: 16398, 16399, 16400, 16401, 16402].

### Next Steps

1.  [cite\_start]**Hardware Integration of `axi_stream_source.v`:** In the Libero SoC FPGA project, the immediate next step is to connect the AXI-Stream output of the `axi_stream_source.v` module to the AXI-Stream slave input of the `CoreAXI4DMAController`[cite: 16405]. This will complete the hardware data path.
2.  **End-to-End System Validation:** Once integrated, use a test bench or a simple counter in the FPGA to generate data and feed it into the packer. [cite\_start]Rerun the `run_stream_to_mem_test` to verify that data is correctly transferred from the FPGA into DDR memory[cite: 16408, 16409, 16410].
3.  [cite\_start]**External I/O Implementation:** After validating the full fabric-to-DDR data path, the system should be configured to accept clock and data signals from the external RadioHound ADC[cite: 16413].

## Development Environment & Toolchain

Setting up the correct development environment is crucial. [cite\_start]A pre-configured VirtualBox VM is the recommended and fastest way to get started, as it contains all necessary licensed software and a configured toolchain[cite: 16419, 16420].

### Option A: Pre-configured Virtual Machine (Recommended)

  * **VM Location:** *[Location of the .ova file should be specified here]*
  * **Credentials:**
      * [cite\_start]**Username:** `vboxuser` [cite: 16423]
      * [cite\_start]**Password:** `1303` [cite: 16424]
  * **Setup:**
    1.  [cite\_start]Install [Oracle VirtualBox](https://www.virtualbox.org/)[cite: 16426].
    2.  [cite\_start]Go to `File > Import Appliance` and select the downloaded `.ova` file[cite: 16427].
    3.  [cite\_start]Start the VM and log in. All tools are pre-installed[cite: 16429, 16430].

### Option B: Manual Installation

[cite\_start]If you prefer a manual setup (Ubuntu 22.04 LTS recommended), install the following tools[cite: 16432]:

1.  **Microchip Libero SoC v2023.2:**
      * [cite\_start]**Download:** Available from Microchip's website (requires a free account)[cite: 16434].
      * **License:** A **Silver-level license** is required. [cite\_start]You can request a free Silver license from Microchip's licensing portal[cite: 16435, 16436].
      * [cite\_start]**Installation:** Run the installer with `sudo`[cite: 16438].
2.  **Microchip SoftConsole v2023.2:**
      * **Download:** Available from the same Libero SoC download page. [cite\_start]This IDE includes the RISC-V GCC toolchain for compiling the C code[cite: 16440, 16441].
3.  **System Dependencies:**
    ```bash
    # Python dependencies
    pip install gitpython pyyaml

    # System packages
    sudo apt install device-tree-compiler
    sudo apt install python-is-python3
    ```
    [cite\_start][cite: 16451, 16452, 16453, 16454, 16455]

## Build and Deployment

### 1\. Building the Gateware (FPGA)

[cite\_start]This project uses the official BeagleV-Fire gateware repository, which provides a robust build system for synthesizing the FPGA fabric, compiling the bootloader (HSS), and generating the necessary Linux Device Tree Overlays[cite: 16447].

1.  [cite\_start]**Source the Environment Script:** Before building, source the setup script from your Libero installation[cite: 16457].
2.  **Clone the Repository:**
    ```bash
    git clone https://openbeagle.org/beaglev-fire/gateware.git
    cd gateware
    ```
    [cite\_start][cite: 16458, 16459]
3.  **Run the Build Script:** The build is defined by a YAML configuration file. To build the custom design for this project:
    ```bash
    python build-bitstream.py ./build-options/my_custom_design.yaml
    ```
    [cite\_start]*(Note: The exact yaml file name should be confirmed in the repository)* [cite: 16460, 16462]

### 2\. Flashing the Gateware

1.  **Transfer Files:** After a successful build, the output files will be in the `bitstream/` directory. [cite\_start]Use `scp` to copy the entire design directory to the BeagleV-Fire[cite: 16469, 16470].
    ```bash
    scp -r ./bitstream/my_custom_fpga_design beagle@[board-ip-address]:/home/beagle
    ```
2.  [cite\_start]**Run the Update Script:** Connect to the board via SSH or UART and run the provided script to flash the gateware[cite: 16472, 16473].
    ```bash
    sudo /usr/share/beagleboard/gateware/change-gateware.sh ./my_custom_fpga_design
    ```
    [cite\_start]Do not power off the board until it has rebooted successfully[cite: 16475].

### 3\. Building and Running the Software

[cite\_start]The C drivers and test application are compiled and run directly on the BeagleV-Fire's RISC-V processor[cite: 16478].

1.  [cite\_start]**Transfer Files:** Copy the `software` directory from the project repository to the board[cite: 16480, 16481].
    ```bash
    scp -r ./software beagle@[board-ip-address]:/home/beagle
    ```
2.  [cite\_start]**Compile:** Navigate to the directory on the board and use the provided Makefile[cite: 16483].
    ```bash
    cd software
    make
    ```
    [cite\_start][cite: 16484]
3.  [cite\_start]**Run:** Execute the compiled test application[cite: 16485].
    ```bash
    sudo ./build/dma_test_app.elf
    ```
    [cite\_start][cite: 16486]

## Troubleshooting Common Issues

  * **Changing IP Address:** The board's IP address may change after flashing new gateware. [cite\_start]Using the UART debug port is the most reliable connection method for diagnostics, though SSH is recommended for file transfers[cite: 16516, 16517, 16518].
  * **Kernel Startup Failure:** A broken or incorrect device tree can cause the system to hang at "Starting kernel...". [cite\_start]To recover, you must interrupt the U-Boot process and manually load the kernel and device tree from eMMC storage[cite: 16520, 16521, 16522, 16523, 16524, 16525, 16526]. [cite\_start]The permanent solution is to fix the device tree in your project and re-flash the gateware[cite: 16527].
  * **Camera Sensor Error:** An `imx219` error may appear during boot. [cite\_start]This relates to the camera port and can be safely ignored for this project[cite: 16511, 16513].

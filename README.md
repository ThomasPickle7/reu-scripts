# RadioHound High-Speed Data Acquisition System

This document provides a comprehensive guide to the design, status, and development process for the RadioHound high-speed data acquisition system. The project's primary goal is to stream ADC data from a RadioHound sensor through the FPGA fabric of a BeagleV-Fire board and into its DDR memory at the highest possible throughput. This serves as the foundational infrastructure to significantly increase the RadioHound project's sampling rate beyond its current 48MHz capability.

## System Architecture

The system is architected to create a direct, high-throughput data path from the external ADC to the main DDR memory, minimizing CPU overhead. This is achieved by leveraging the FPGA for data packing and transfer, with the CPU's role limited to configuration and control.

The architecture is centered around Microchip's `CoreAXI4DMAController` IP, which is capable of achieving transfer rates up to 7488 Mb/s.

### Core Components

#### Hardware (FPGA Fabric)

  * **I/O (ADC):** An external 8-bit ADC from the RadioHound sensor board provides the raw data samples.
  * **Stream Packer (`axi_stream_source.v`):** A custom Verilog module that aggregates eight 8-bit samples into a single 64-bit word. This packing is essential to maximize the efficiency of the 64-bit AXI4 bus. It is controlled via memory-mapped registers at base address `0x60000000`.
  * **DMA Interface (`CoreAXI4DMAController`):** This Microchip IP core manages the data movement. It reads data from the AXI4-Stream source (the packer) and writes it to the DDR memory. Its control registers are located at base address `0x60010000`.
  * **AXI Interconnect (`CoreAXI4Interconnect`):** This IP core acts as the central bus matrix, connecting all FPGA peripherals to the CPU's memory subsystem and defining the memory map for the FPGA design.

#### Software & Memory (CPU Stack)

  * **DDR Memory:** The main system memory. DMA operations specifically target a **non-cached** memory region starting at `0xC0000000` to prevent CPU cache coherency issues and maximize throughput.
  * **C Drivers:** A suite of bare-metal C files that configure and control the hardware components.
      * **`mpu_driver.c`:** This critical driver configures the **Memory Protection Unit (MPU)**. It grants the DMA controller (an AXI master) the necessary permissions to read and write to the non-cached DDR memory space. Without this configuration, all DMA transfers would fail.
      * **`dma_driver.c`:** This driver provides the API to initialize, configure, and manage the `CoreAXI4DMAController`, primarily by setting up DMA descriptors in memory.

## Project Status

### Completed & Verified

  * **MPU Driver (`mpu_driver.c`):** Correctly configures the MPU to grant the FPGA fabric access to the non-cached DDR memory range `0xC0000000` - `0xDFFFFFFF`.
  * **DMA Driver (`dma_driver.c`):** Core functions for DMA initialization and descriptor setup are implemented and stable.
  * **Memory-to-Memory DMA Test:** The `run_mem_to_mem_ping_pong` test in `main.c` passes successfully. This test verifies that the MPU is configured correctly, the CPU can communicate with the DMA, and the DMA can read from and write to DDR memory.

### In Progress

  * **Stream-to-Memory DMA Test:** The software for this test (`run_stream_to_mem_test`) is implemented and correctly configures the DMA to receive a stream. However, the test currently **hangs** because the DMA controller is waiting for the `TVALID` AXI-Stream signal from a hardware source. This signal is never asserted because the `axi_stream_source.v` packer is not yet connected to the DMA controller in the FPGA design. This is the project's primary bottleneck.

### Next Steps

1.  **Hardware Integration of `axi_stream_source.v`:** In the Libero SoC FPGA project, the immediate next step is to connect the AXI-Stream output of the `axi_stream_source.v` module to the AXI-Stream slave input of the `CoreAXI4DMAController`. This will complete the hardware data path.
2.  **End-to-End System Validation:** Once integrated, use a test bench or a simple counter in the FPGA to generate data and feed it into the packer. Rerun the `run_stream_to_mem_test` to verify that data is correctly transferred from the FPGA into DDR memory.
3.  **External I/O Implementation:** After validating the full fabric-to-DDR data path, the system should be configured to accept clock and data signals from the external RadioHound ADC.

## Development Environment & Toolchain

Setting up the correct development environment is crucial. A pre-configured VirtualBox VM is the recommended and fastest way to get started, as it contains all necessary licensed software and a configured toolchain.

### Option A: Pre-configured Virtual Machine (Recommended)

  * **VM Location:** *[Location of the .ova file should be specified here]*
  * **Credentials:**
      * **Username:** `vboxuser`
      * **Password:** `1303`
  * **Setup:**
    1.  Install [Oracle VirtualBox](https://www.virtualbox.org/).
    2.  Go to `File > Import Appliance` and select the downloaded `.ova` file.
    3.  Start the VM and log in. All tools are pre-installed.

### Option B: Manual Installation

If you prefer a manual setup (Ubuntu 22.04 LTS recommended), install the following tools:

1.  **Microchip Libero SoC v2023.2:**
      * **Download:** Available from Microchip's website (requires a free account).
      * **License:** A **Silver-level license** is required. You can request a free Silver license from Microchip's licensing portal.
      * **Installation:** Run the installer with `sudo`.
2.  **Microchip SoftConsole v2023.2:**
      * **Download:** Available from the same Libero SoC download page. This IDE includes the RISC-V GCC toolchain for compiling the C code.
3.  **System Dependencies:**
    ```bash
    # Python dependencies
    pip install gitpython pyyaml

    # System packages
    sudo apt install device-tree-compiler
    sudo apt install python-is-python3
    ```

## Build and Deployment

### 1\. Building the Gateware (FPGA)

This project uses the official BeagleV-Fire gateware repository, which provides a robust build system for synthesizing the FPGA fabric, compiling the bootloader (HSS), and generating the necessary Linux Device Tree Overlays.

1.  **Source the Environment Script:** Before building, source the setup script from your Libero installation.
2.  **Clone the Repository:**
    ```bash
    git clone https://openbeagle.org/beaglev-fire/gateware.git
    cd gateware
    ```
3.  **Run the Build Script:** The build is defined by a YAML configuration file. To build the custom design for this project:
    ```bash
    python build-bitstream.py ./build-options/my_custom_design.yaml
    ```
    *(Note: The exact yaml file name should be confirmed in the repository)*

### 2\. Flashing the Gateware

1.  **Transfer Files:** After a successful build, the output files will be in the `bitstream/` directory. Use `scp` to copy the entire design directory to the BeagleV-Fire.
    ```bash
    scp -r ./bitstream/my_custom_fpga_design beagle@[board-ip-address]:/home/beagle
    ```
2.  **Run the Update Script:** Connect to the board via SSH or UART and run the provided script to flash the gateware.
    ```bash
    sudo /usr/share/beagleboard/gateware/change-gateware.sh ./my_custom_fpga_design
    ```
    Do not power off the board until it has rebooted successfully.

### 3\. Building and Running the Software

The C drivers and test application are compiled and run directly on the BeagleV-Fire's RISC-V processor.

1.  **Transfer Files:** Copy the `software` directory from the project repository to the board.
    ```bash
    scp -r ./software beagle@[board-ip-address]:/home/beagle
    ```
2.  **Compile:** Navigate to the directory on the board and use the provided Makefile.
    ```bash
    cd software
    make
    ```
3.  **Run:** Execute the compiled test application.
    ```bash
    sudo ./build/dma_test_app.elf
    ```

## Troubleshooting Common Issues

  * **Changing IP Address:** The board's IP address may change after flashing new gateware. Using the UART debug port is the most reliable connection method for diagnostics, though SSH is recommended for file transfers.
  * **Kernel Startup Failure:** A broken or incorrect device tree can cause the system to hang at "Starting kernel...". To recover, you must interrupt the U-Boot process and manually load the kernel and device tree from eMMC storage. The permanent solution is to fix the device tree in your project and re-flash the gateware.
  * **Camera Sensor Error:** An `imx219` error may appear during boot. This relates to the camera port and can be safely ignored for this project.

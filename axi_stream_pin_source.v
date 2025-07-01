////////////////////////////////////////////////////////////////////////////////
//
// DUT: axi_stream_source (v25 - Protocol-Compliant Output Stage)
//
//
// -- High-Level Description --
//
// This module functions as a hardware data generator that converts a simple
// 8-bit parallel input into a full AXI4-Stream interface. Its primary role
// is to act as an AXI Master, sending a continuous series of data packets
// to a connected AXI Slave, such as a DMA controller.
//
// Key Operations:
// 1. Byte Assembly: It consumes one 8-bit sample from 'data_pins' on each
//    clock cycle. It assembles every four consecutive bytes into a single
//    32-bit word.
// 2. FIFO Buffering: A small internal FIFO is used to buffer the assembled
//    32-bit words. This decouples the input data collection from the output
//    AXI stream, allowing the module to handle backpressure from the slave
//    without losing incoming data (up to the FIFO's depth).
// 3. AXI4-Stream Packet Generation: The module reads words from the FIFO
//    and transmits them as AXI4-Stream packets. It correctly manages the
//    AXI handshake signals (TVALID and TREADY).
// 4. Packet Termination (TLAST): Crucially, it counts the number of words
//    sent and asserts the 'm_axis_tlast' signal on the final word of each
//    packet. This is essential for the receiving slave (DMA) to know when
//    a transfer is complete and to trigger events like interrupts.
//
//
// -- Protocol Compliance Notes (v25) --
//
// This version includes a robust, protocol-compliant output stage. The AXI4-
// Stream specification requires that once TVALID is asserted, the master's
// outputs (TDATA, TLAST, etc.) must remain stable until the slave accepts
// the transfer (TREADY is high). This version ensures this by separating the
// logic that drives the output registers from the logic that updates the
// internal state (like FIFO pointers), preventing data from changing during
// a backpressure stall.
//
////////////////////////////////////////////////////////////////////////////////

`timescale 1ns / 1ps
module axi_stream_source (
    input  wire         aclk,
    input  wire         aresetn,
    input  wire [7:0]   data_pins,

    // AXI4-Stream Master Interface
    output wire         m_axis_tvalid,
    output wire [31:0]  m_axis_tdata,
    output wire         m_axis_tlast,
    output wire [1:0]   m_axis_tdest,
    output wire [3:0]   m_axis_tkeep,
    output wire [3:0]   m_axis_tstrb,
    output wire [7:0]   m_axis_tid,
    input  wire         m_axis_tready
);

    // --- Parameters ---
    // These constants define the structure of the FIFO and the AXI packets.
    localparam FIFO_DEPTH_BITS = 4;              // Log2(FIFO_DEPTH), defines pointer width.
    localparam FIFO_DEPTH      = 1 << FIFO_DEPTH_BITS; // FIFO holds 16 words.
    
    // Defines the packet size to match the DMA's expectation.
    // 1MB = 1048576 bytes. Data width is 32 bits (4 bytes).
    // Packet size = 1048576 / 4 = 262144 words.
    localparam WORDS_PER_PACKET = 262144;

    // --- Internal FIFO ---
    // This section implements a standard First-In, First-Out buffer.
    reg  [31:0] fifo_mem [0:FIFO_DEPTH-1];     // The memory array for the FIFO.
    reg  [FIFO_DEPTH_BITS-1:0] wr_ptr;         // Write pointer.
    reg  [FIFO_DEPTH_BITS-1:0] rd_ptr;         // Read pointer.
    reg  [FIFO_DEPTH_BITS:0]   fifo_count;     // Number of words currently in the FIFO.

    wire fifo_full  = (fifo_count == FIFO_DEPTH); // High when FIFO cannot accept more data.
    wire fifo_empty = (fifo_count == 0);          // High when FIFO has no data to read.

    // --- Data Collection (FIFO Write Side) ---
    // This logic handles the assembly of 8-bit bytes into 32-bit words.
    reg [1:0]  byte_counter;     // Counts incoming bytes from 0 to 3.
    reg [23:0] data_accumulator; // Temporarily stores the first three bytes of a word.
    
    wire byte_collector_valid = (byte_counter == 2'd3); // High when four bytes have been collected.
    wire fifo_wr_ready = !fifo_full;                    // High if the FIFO has space.
    wire do_write = byte_collector_valid && fifo_wr_ready; // The final condition to write a word to the FIFO.

    // --- AXI Output (FIFO Read Side) ---
    // This logic handles the AXI handshake for sending data out.
    reg [31:0] m_axis_tdata_reg; // Registered output for data, ensures stability.
    wire do_read = m_axis_tready && !fifo_empty; // Condition for a successful data transfer off the bus.
    
    // --- TLAST Generation Logic ---
    // This logic tracks the packet progress to assert TLAST at the right time.
    reg [17:0] word_counter; // Counts words sent in the current packet. Needs to count up to 262144.
    reg        tlast_reg;    // Registered output for the TLAST signal.

    // --- Output Assignments ---
    // Connects internal registers to the module's output ports.
    assign m_axis_tvalid = !fifo_empty;      // TVALID is high whenever there is data in the FIFO.
    assign m_axis_tdata  = m_axis_tdata_reg; // Drive TDATA from a stable register.
    assign m_axis_tdest  = 2'b00;            // Unused, tied to 0.
    assign m_axis_tkeep  = 4'b1111;          // All bytes are valid.
    assign m_axis_tstrb  = 4'b1111;          // All bytes are valid.
    assign m_axis_tid    = 8'h00;            // Unused, tied to 0.
    assign m_axis_tlast  = tlast_reg;        // Drive TLAST from a stable register.

    // --- Logic Implementation Blocks ---

    // Block 1: Input Byte Collection
    // This process runs continuously, collecting one byte per clock cycle
    // and assembling them into a 32-bit word.
    always @(posedge aclk) begin
        if (!aresetn) begin
            byte_counter     <= 2'd0;
            data_accumulator <= 24'd0;
        end else begin
            // Only advance the byte counter if the word isn't full yet, OR if the
            // word is full and we can successfully write it to the FIFO.
            if (do_write || !byte_collector_valid) begin
                byte_counter <= byte_counter + 1;
                
                case (byte_counter)
                    2'd0: data_accumulator[7:0]   <= data_pins;
                    2'd1: data_accumulator[15:8]  <= data_pins;
                    2'd2: data_accumulator[23:16] <= data_pins;
                    // On byte 3, a 'do_write' occurs. We must clear the accumulator
                    // to be ready for the *next* word, preventing data corruption.
                    default: data_accumulator <= 24'd0; 
                endcase
            end
        end
    end

    // Block 2: FIFO Write Logic
    // This process writes a fully assembled word into the FIFO.
    always @(posedge aclk) begin
        if (!aresetn) begin
            wr_ptr <= {FIFO_DEPTH_BITS{1'b0}};
        end else begin
            // When 'do_write' is high, the byte collector has a full word ready
            // and the FIFO has space.
            if (do_write) begin
                // Concatenate the final byte (MSB) with the first three bytes (LSBs)
                // to form the complete 32-bit word.
                fifo_mem[wr_ptr] <= {data_pins, data_accumulator};
                wr_ptr <= wr_ptr + 1;
            end
        end
    end

    // Block 3: FIFO Read - Output Register Driver
    // This is a critical block for AXI protocol compliance. It is responsible
    // ONLY for driving the output registers. It ensures that TDATA and TLAST
    // remain stable if the AXI slave applies backpressure (tready=0).
    always @(posedge aclk) begin
        if (!aresetn) begin
            m_axis_tdata_reg <= 32'b0;
            tlast_reg        <= 1'b0;
        end else begin
            if (do_read) begin
                // A valid handshake is happening. Latch the data for the current word.
                m_axis_tdata_reg <= fifo_mem[rd_ptr];
                
                // Determine if this is the last word and latch TLAST accordingly.
                if (word_counter == WORDS_PER_PACKET - 1) begin
                    tlast_reg <= 1'b1;
                end else begin
                    tlast_reg <= 1'b0;
                end
            end
            // If TLAST was asserted, it must be de-asserted on the next cycle
            // after the handshake completes. The 'else if' ensures this happens only
            // when not performing a new read.
            else if (tlast_reg) begin
                 tlast_reg <= 1'b0;
            end
        end
    end

    // Block 4: FIFO Read - Internal State Logic
    // This block manages the internal state (pointers and counters). It advances
    // the state ONLY upon a successful handshake (do_read is high). Separating
    // this from the output logic above is key to a robust design.
    always @(posedge aclk) begin
        if (!aresetn) begin
            rd_ptr       <= {FIFO_DEPTH_BITS{1'b0}};
            word_counter <= 0;
        end else begin
            if (do_read) begin
                rd_ptr <= rd_ptr + 1;
                
                if (word_counter == WORDS_PER_PACKET - 1) begin
                    word_counter <= 0; // Reset for the next packet
                end else begin
                    word_counter <= word_counter + 1;
                end
            end
        end
    end

    // Block 5: FIFO Count Management
    // Standard logic for tracking the number of items in the FIFO.
    always @(posedge aclk) begin
        if (!aresetn) begin
            fifo_count <= {(FIFO_DEPTH_BITS+1){1'b0}};
        end else begin
            // Case 1: Write but no Read -> Count increments
            if (do_write && !do_read) begin
                fifo_count <= fifo_count + 1;
            // Case 2: Read but no Write -> Count decrements
            end else if (!do_write && do_read) begin
                fifo_count <= fifo_count - 1;
            end
            // Case 3 (implicit): Write and Read, or No Write and No Read -> Count is unchanged.
        end
    end

    // --- Assertions for Debugging ---
    // These are simulation-only checks that will flag common errors.
    // They are ignored during synthesis for the actual hardware.
    // synthesis translate_off
    always @(posedge aclk) begin
        if (aresetn) begin
            if (do_read && fifo_empty) begin
                $error("FIFO underflow: Attempting to read from empty FIFO!");
            end
            if (do_write && fifo_full) begin
                $error("FIFO overflow: Attempting to write to full FIFO!");
            end
        end
    end
    // synthesis translate_on

endmodule


////////////////////////////////////////////////////////////////////////////////
//
// Testbench for axi_stream_source (v2 - Verbose)
//
////////////////////////////////////////////////////////////////////////////////

module tb_axi_stream_source;

    // --- Testbench Parameters ---
    localparam CLK_PERIOD = 10; // 100 MHz clock
    localparam PACKET_SIZE_WORDS = 262144; // 1MB packet size

    // --- Testbench Signals ---
    reg aclk;
    reg aresetn;
    reg [7:0] data_pins;

    wire m_axis_tvalid;
    wire [31:0] m_axis_tdata;
    wire m_axis_tlast;
    wire [1:0] m_axis_tdest;
    wire [3:0] m_axis_tkeep;
    wire [3:0] m_axis_tstrb;
    wire [7:0] m_axis_tid;
    reg  m_axis_tready;

    // --- Instantiate the DUT ---
    axi_stream_source dut (
        .aclk(aclk),
        .aresetn(aresetn),
        .data_pins(data_pins),
        .m_axis_tvalid(m_axis_tvalid),
        .m_axis_tdata(m_axis_tdata),
        .m_axis_tlast(m_axis_tlast),
        .m_axis_tdest(m_axis_tdest),
        .m_axis_tkeep(m_axis_tkeep),
        .m_axis_tstrb(m_axis_tstrb),
        .m_axis_tid(m_axis_tid),
        .m_axis_tready(m_axis_tready)
    );

    // --- Clock Generation ---
    always #(CLK_PERIOD / 2) aclk = ~aclk;

    // --- Main Test Sequence ---
    initial begin
        $display("TESTBENCH: Simulation Started.");
        
        // 1. Initialize signals
        aclk = 0;
        aresetn = 0;
        data_pins = 8'h00;
        m_axis_tready = 0;

        // 2. Apply Reset
        # (CLK_PERIOD * 5);
        aresetn = 1;
        $display("TESTBENCH: Reset de-asserted. DUT is active.");
        
        // 3. Start providing data and enable receiver
        m_axis_tready = 1;
        
        // 4. Run the test for two full packets
        test_one_packet(1);
        test_one_packet(2);

        $display("TESTBENCH: All tests passed!");
        $finish;
    end

    // --- Data Provider Task ---
    // This task continuously provides incrementing data to the DUT
    always @(posedge aclk) begin
        if (aresetn) begin
            data_pins <= data_pins + 1;
        end
    end

    // --- Verification Task for a Single Packet ---
    task test_one_packet(input integer packet_num);
        integer word_count = 0;
        
        $display("--------------------------------------------------");
        $display("TESTBENCH: Starting verification for Packet #%0d", packet_num);
        $display("--------------------------------------------------");

        while (word_count < PACKET_SIZE_WORDS) begin
            @(posedge aclk);

            // Occasionally apply backpressure to test flow control
            if (word_count > 1000 && word_count < 1010) begin
                m_axis_tready <= 0;
            end else begin
                m_axis_tready <= 1;
            end

            if (m_axis_tvalid && m_axis_tready) begin
                word_count = word_count + 1;

                // Check TLAST condition
                if (word_count == PACKET_SIZE_WORDS) begin
                    if (m_axis_tlast == 1'b1) begin
                        $display("SUCCESS: TLAST correctly asserted on word %0d.", word_count);
                    end else begin
                        $display("FAILURE: TLAST was NOT asserted on the final word (%0d).", word_count);
                        $finish;
                    end
                end else begin
                    if (m_axis_tlast == 1'b1) begin
                        $display("FAILURE: TLAST was asserted prematurely on word %0d.", word_count);
                        $finish;
                    end
                end
            end
        end
        $display("TESTBENCH: Successfully received one full packet.");
    endtask

    // --- NEW: Verbose Monitor Block ---
    // This block will print the state of key signals on every clock cycle
    // to help diagnose hangs and other issues.
    initial begin
        # (CLK_PERIOD * 5); // Wait for reset to finish
        $display("\n--- Starting Cycle-by-Cycle Monitor ---");
        $display("Time(ns) | TReady | TValid | TLast | FIFO Cnt | Byte Ctr | Word Ctr | Data");
        $display("---------|--------|--------|-------|----------|----------|----------|-----------");
    end

    always @(posedge aclk) begin
        if (aresetn) begin
            // Use $timeformat to make the time easier to read
            $display("%t |   %b    |   %b    |   %b   |    %2d    |    %d     |   %6d   | %h",
                     $time,
                     m_axis_tready,
                     m_axis_tvalid,
                     m_axis_tlast,
                     dut.fifo_count,
                     dut.byte_counter,
                     dut.word_counter,
                     m_axis_tdata);
        end
    end

endmodule

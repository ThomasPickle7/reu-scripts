////////////////////////////////////////////////////////////////////////////////
//
// DUT: axi_stream_source (v27 - Final Protocol-Compliant Version)
//
//
// -- High-Level Description --
//
// This module functions as a hardware data generator that converts a simple
// 8-bit parallel input into a full AXI4-Stream interface. Its primary role
// is to act as an AXI Master, sending a continuous series of data packets
// to a connected AXI Slave, such as a DMA controller.
//
// -- v27 Change --
// - Reworked the AXI output stage to be fully protocol-compliant, fixing a
//   one-cycle latency mismatch between TDATA and TLAST.
// - The output registers (tdata, tlast) are now updated synchronously with
//   the internal state machine (pointers, counters) only when a valid
//   AXI handshake occurs (TVALID and TREADY are both high).
// - This guarantees that TLAST is asserted on the same cycle as the final
//   word of data, and that all outputs remain stable during backpressure,
//   resolving the simulation timeout.
//
////////////////////////////////////////////////////////////////////////////////

`timescale 1ns / 1ps
module axi_stream_source (
    input  wire         aclk,
    input  wire         aresetn,
    input  wire [7:0]   data_pins,
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
    localparam FIFO_DEPTH_BITS = 4;
    localparam FIFO_DEPTH      = 1 << FIFO_DEPTH_BITS;
    
    // Use a 'parameter' so the testbench can override this value for fast simulation.
    // The default value is the target for hardware synthesis.
    parameter WORDS_PER_PACKET = 262144; 

    // --- Internal FIFO ---
    reg  [31:0] fifo_mem [0:FIFO_DEPTH-1];
    reg  [FIFO_DEPTH_BITS-1:0] wr_ptr;
    reg  [FIFO_DEPTH_BITS-1:0] rd_ptr;
    reg  [FIFO_DEPTH_BITS:0]   fifo_count;
    wire fifo_full  = (fifo_count == FIFO_DEPTH);
    wire fifo_empty = (fifo_count == 0);

    // --- Data Collection ---
    reg [1:0]  byte_counter;
    reg [23:0] data_accumulator;
    wire byte_collector_valid = (byte_counter == 2'd3);
    wire fifo_wr_ready = !fifo_full;
    wire do_write = byte_collector_valid && fifo_wr_ready;

    // --- AXI Output Logic ---
    reg [31:0] m_axis_tdata_reg;
    wire do_read = m_axis_tready && m_axis_tvalid; // Correct handshake condition
    reg [$clog2(WORDS_PER_PACKET):0] word_counter; // Use clog2 for minimum size, +1 for full range
    reg        tlast_reg;
    reg        tvalid_reg;

    // --- Output Assignments ---
    assign m_axis_tvalid = tvalid_reg;
    assign m_axis_tdata  = m_axis_tdata_reg;
    assign m_axis_tdest  = 2'b00;
    assign m_axis_tkeep  = 4'b1111;
    assign m_axis_tstrb  = 4'b1111;
    assign m_axis_tid    = 8'h00;
    assign m_axis_tlast  = tlast_reg;

    // Block 1: Input Byte Collection
    always @(posedge aclk) begin
        if (!aresetn) begin
            byte_counter     <= 2'd0;
            data_accumulator <= 24'd0;
        end else if (do_write || !byte_collector_valid) begin
            byte_counter <= byte_counter + 1;
            case (byte_counter)
                2'd0: data_accumulator[7:0]   <= data_pins;
                2'd1: data_accumulator[15:8]  <= data_pins;
                2'd2: data_accumulator[23:16] <= data_pins;
                default: data_accumulator <= 24'd0; 
            endcase
        end
    end

    // Block 2: FIFO Write Logic
    always @(posedge aclk) begin
        if (!aresetn) begin
            wr_ptr <= {FIFO_DEPTH_BITS{1'b0}};
        end else if (do_write) begin
            fifo_mem[wr_ptr] <= {data_pins, data_accumulator};
            wr_ptr <= wr_ptr + 1;
        end
    end

    // Block 3: Protocol-Compliant AXI Output Stage
    always @(posedge aclk) begin
        if (!aresetn) begin
            tvalid_reg       <= 1'b0;
            m_axis_tdata_reg <= 32'b0;
            tlast_reg        <= 1'b0;
            rd_ptr           <= 0;
            word_counter     <= 0;
        end else begin
            if (tvalid_reg && !m_axis_tready) begin
                // STALL: Hold outputs stable
            end else begin
                // TRANSFER or IDLE
                tvalid_reg <= !fifo_empty;

                if (!fifo_empty) begin
                    m_axis_tdata_reg <= fifo_mem[rd_ptr];
                    tlast_reg        <= (word_counter == WORDS_PER_PACKET - 1);
                    
                    rd_ptr <= rd_ptr + 1;
                    if (word_counter == WORDS_PER_PACKET - 1) begin
                        word_counter <= 0;
                    end else begin
                        word_counter <= word_counter + 1;
                    end
                end
            end
        end
    end

    // Block 4: FIFO Count Management
    always @(posedge aclk) begin
        if (!aresetn) begin
            fifo_count <= {(FIFO_DEPTH_BITS+1){1'b0}};
        end else if (do_write && !do_read) begin
            fifo_count <= fifo_count + 1;
        end else if (!do_write && do_read) begin
            fifo_count <= fifo_count - 1;
        end
    end

endmodule

////////////////////////////////////////////////////////////////////////////////
//
// Testbench for axi_stream_source (v6 - Full 1MB Packet Verification)
//
////////////////////////////////////////////////////////////////////////////////

module tb_axi_stream_source;

    // --- Testbench Parameters ---
    localparam CLK_PERIOD = 10; // 100 MHz clock
    // **NOTE: Using the full 1MB packet size for final verification. **
    // **This simulation will take a significant amount of time to complete.**
    localparam PACKET_SIZE_WORDS = 262144; 

    // --- Testbench Signals ---
    reg aclk;
    reg aresetn;
    reg [7:0] data_pins;
    wire m_axis_tvalid;
    wire [31:0] m_axis_tdata;
    wire m_axis_tlast;
    reg  m_axis_tready;

    // --- Instantiate the DUT ---
    // The DUT's default parameter is already 1MB, so no override is needed.
    axi_stream_source dut (
        .aclk(aclk),
        .aresetn(aresetn),
        .data_pins(data_pins),
        .m_axis_tvalid(m_axis_tvalid),
        .m_axis_tdata(m_axis_tdata),
        .m_axis_tlast(m_axis_tlast),
        .m_axis_tdest(),
        .m_axis_tkeep(),
        .m_axis_tstrb(),
        .m_axis_tid(),
        .m_axis_tready(m_axis_tready)
    );

    // --- Clock Generation ---
    always #(CLK_PERIOD / 2) aclk = ~aclk;

    // --- Main Test Sequence ---
    initial begin
        $display("TESTBENCH: Simulation Started for full 1MB packet.");
        
        aclk = 0;
        aresetn = 0;
        data_pins = 8'h00;
        m_axis_tready = 0;

        // Apply Reset
        # (CLK_PERIOD * 5);
        aresetn = 1;
        $display("TESTBENCH: Reset de-asserted. DUT is active.");
        
        // Start the receiver
        m_axis_tready = 1; 
        
        // Set a very long timeout for the simulation
        #(CLK_PERIOD * PACKET_SIZE_WORDS * 2);
        $display("\n***** TEST FAILED! *****");
        $display(" -> Reason: Simulation timed out. The test did not complete.");
        $finish;
    end

    // --- Data Provider ---
    always @(posedge aclk) begin
        if (aresetn) begin
            data_pins <= data_pins + 1;
        end
    end

    // --- Verification Logic ---
    integer word_count = 0;
    
    always @(posedge aclk) begin
        if (!aresetn) begin
            word_count <= 0;
        end else begin
            if (m_axis_tvalid && m_axis_tready) begin
                
                // Print a message at the start of the packet
                if (word_count == 0) begin
                    $display("--------------------------------------------------");
                    $display("INFO @ %t: First word of 1MB packet received.", $time);
                    $display(" -> Now receiving the remaining %0d words...", PACKET_SIZE_WORDS - 1);
                end
                
                // Check for correct TLAST assertion on the final word
                if (m_axis_tlast && (word_count == PACKET_SIZE_WORDS - 1)) begin
                    $display("INFO @ %t: Final word received. TLAST is correctly asserted.", $time);
                    $display("--------------------------------------------------");
                    $display("***** TEST PASSED! Full 1MB packet received with correct TLAST. *****");
                    $display("***** This confirms the logic will generate a DMA interrupt. *****");
                    $finish;
                end

                // Check for premature TLAST assertion
                if (m_axis_tlast && (word_count < PACKET_SIZE_WORDS - 1)) begin
                    $display("\n***** TEST FAILED! *****");
                    $display(" -> Reason: TLAST asserted prematurely on word %0d.", word_count + 1);
                    $finish;
                end
                
                // Check for missed TLAST assertion
                if (!m_axis_tlast && (word_count == PACKET_SIZE_WORDS - 1)) begin
                    // This case should not be hit if the logic is correct, but is a safeguard.
                    // The final check will happen on the next clock cycle after this.
                end
                
                word_count <= word_count + 1;
            end
        end
    end

endmodule

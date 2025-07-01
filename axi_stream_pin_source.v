////////////////////////////////////////////////////////////////////////////////
//
// DUT: axi_stream_source (v23 - Corrected and Robust Version)
//
// Description:
// This module captures an 8-bit parallel data input on every clock cycle,
// assembles it into 32-bit words, and streams it out over a standard
// AXI4-Stream master interface. It uses a FIFO to buffer data between the
// input and output domains, correctly handling backpressure from the AXI slave.
//
// Corrections from previous version:
// 1. Fixed data corruption bug by clearing the data_accumulator after each word.
// 2. Registered the FIFO output to enable efficient Block RAM synthesis.
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
    localparam FIFO_DEPTH_BITS = 4;
    localparam FIFO_DEPTH      = 1 << FIFO_DEPTH_BITS;

    // --- Internal FIFO ---
    reg  [31:0] fifo_mem [0:FIFO_DEPTH-1];
    reg  [FIFO_DEPTH_BITS-1:0] wr_ptr;
    reg  [FIFO_DEPTH_BITS-1:0] rd_ptr;
    reg  [FIFO_DEPTH_BITS:0]   fifo_count;

    wire fifo_full  = (fifo_count == FIFO_DEPTH);
    wire fifo_empty = (fifo_count == 0);

    // --- Data Collection (FIFO Write Side) ---
    reg [1:0]  byte_counter;
    reg [23:0] data_accumulator;
    
    wire byte_collector_valid = (byte_counter == 2'd3);
    wire fifo_wr_ready = !fifo_full;
    wire do_write = byte_collector_valid && fifo_wr_ready;

    // --- AXI Output (FIFO Read Side) ---
    // ** FIX 2: Added output register for proper BRAM inference **
    reg [31:0] m_axis_tdata_reg; 
    wire do_read = m_axis_tready && !fifo_empty;

    // --- Output Assignments ---
    assign m_axis_tvalid = !fifo_empty;
    assign m_axis_tdata  = m_axis_tdata_reg; // Assign from the register
    assign m_axis_tdest  = 2'b00;
    assign m_axis_tkeep  = 4'b1111; // All bytes are valid
    assign m_axis_tstrb  = 4'b1111; // All bytes are valid
    assign m_axis_tid    = 8'h00;
    assign m_axis_tlast  = 1'b0;    // This module generates an infinite stream

    // Input Byte Collection Logic
    always @(posedge aclk) begin
        if (!aresetn) begin
            byte_counter     <= 2'd0;
            data_accumulator <= 24'd0;
        end else begin
            // Advance the counter if the word isn't full,
            // or if it is full and the FIFO is ready for it.
            if (do_write || !byte_collector_valid) begin
                byte_counter <= byte_counter + 1;
                
                case (byte_counter)
                    2'd0: data_accumulator[7:0]   <= data_pins;
                    2'd1: data_accumulator[15:8]  <= data_pins;
                    2'd2: data_accumulator[23:16] <= data_pins;
                    // ** FIX 1: When byte_counter is 3, a 'do_write' is occurring.
                    // We must clear the accumulator so it's ready for the *next* word.
                    // This happens on the same clock edge the full word is written.
                    default: data_accumulator <= 24'd0; 
                endcase
            end
        end
    end

    // FIFO Write Logic
    always @(posedge aclk) begin
        if (!aresetn) begin
            wr_ptr <= {FIFO_DEPTH_BITS{1'b0}};
        end else begin
            if (do_write) begin
                // The fourth byte from data_pins and the first three from the accumulator
                // are combined and written into the FIFO memory.
                fifo_mem[wr_ptr] <= {data_pins, data_accumulator};
                wr_ptr <= wr_ptr + 1;
            end
        end
    end

    // FIFO Read Logic
    always @(posedge aclk) begin
        if (!aresetn) begin
            rd_ptr <= {FIFO_DEPTH_BITS{1'b0}};
        end else begin
            if (do_read) begin
                rd_ptr <= rd_ptr + 1;
            end
        end
        // ** FIX 2: Continuously read from the FIFO memory into the output register. **
        // This structure is what allows the synthesizer to infer a Block RAM.
        // The data for a given read will appear on m_axis_tdata in the cycle AFTER do_read.
        m_axis_tdata_reg <= fifo_mem[rd_ptr];
    end

    // FIFO Count Management
    always @(posedge aclk) begin
        if (!aresetn) begin
            fifo_count <= {(FIFO_DEPTH_BITS+1){1'b0}};
        end else begin
            // Use 'else if' to correctly handle the case where both read and write happen
            // simultaneously (count does not change).
            if (do_write && !do_read) begin
                fifo_count <= fifo_count + 1;
            end else if (!do_write && do_read) begin
                fifo_count <= fifo_count - 1;
            end
        end
    end

    // Assertions for debugging (ignored by synthesis)
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

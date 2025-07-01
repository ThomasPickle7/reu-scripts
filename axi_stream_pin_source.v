// SPDX-License-Identifier: MIT
/*
 * AXI4-Stream source for testing the PolarFire SoC DMA.
 *
 * v16: Made module IO strictly AXI4-Stream compliant.
 * Changes from v15:
 * - Removed non-AXI status/control signals from the IO:
 * - input_ready
 * - fifo_full_flag
 * - fifo_empty_flag
 * - The module now has a purely AXI4-Stream master interface, plus the
 * data input pins. Upstream logic must ensure data is provided at a
 * rate the downstream AXI bus can handle, as there is no longer a
 * backpressure signal. Data will be dropped if the input rate is
 * too high and the internal FIFO becomes full.
 */
module axi_stream_source (
    input  wire         aclk,
    input  wire         aresetn,

    // Data Input
    input  wire [7:0]   data_pins,

    // AXI4-Stream Master Interface (Output)
    output reg          m_axis_tvalid,
    output wire [31:0]  m_axis_tdata,
    output wire         m_axis_tlast,
    output wire [1:0]   m_axis_tdest,
    output wire [3:0]   m_axis_tkeep,
    output wire [3:0]   m_axis_tstrb,
    output wire [7:0]   m_axis_tid,
    input  wire         m_axis_tready
);

    // --- Parameters ---
    localparam FIFO_DEPTH_BITS = 4;              // FIFO depth (2^4 = 16 words)
    localparam FIFO_DEPTH      = 1 << FIFO_DEPTH_BITS;

    // --- FIFO Registers and Wires ---
    reg  [31:0] fifo_mem [0:FIFO_DEPTH-1];
    reg  [FIFO_DEPTH_BITS-1:0] wr_ptr;
    reg  [FIFO_DEPTH_BITS-1:0] rd_ptr;
    reg  [FIFO_DEPTH_BITS:0]   fifo_count; // One extra bit for full/empty distinction

    // FIFO status signals (internal)
    wire fifo_full  = (fifo_count == FIFO_DEPTH);
    wire fifo_empty = (fifo_count == 0);

    // Control signals
    wire do_read  = m_axis_tvalid && m_axis_tready;
    wire do_write = !fifo_full && (byte_counter == 2'd3);

    // Next state calculations for FIFO
    wire [FIFO_DEPTH_BITS:0] next_fifo_count = 
        (do_write && !do_read) ? fifo_count + 1 :
        (!do_write && do_read) ? fifo_count - 1 :
        fifo_count; // No change for simultaneous or no operations

    // --- Data Collection Registers ---
    reg [1:0]  byte_counter;
    reg [23:0] data_accumulator;
    reg [31:0] assembled_word;

    // --- Tie off unused/static signals ---
    assign m_axis_tdest = 2'b00;
    assign m_axis_tkeep = 4'b1111;
    assign m_axis_tstrb = 4'b1111;
    assign m_axis_tid   = 8'h00;

    // --- Output Assignments ---
    assign m_axis_tdata = fifo_mem[rd_ptr];
    assign m_axis_tlast = 1'b0;  // Never assert TLAST for continuous stream

    // --- Main Logic ---
    always @(posedge aclk) begin
        if (!aresetn) begin
            // Reset data collection
            byte_counter     <= 2'd0;
            data_accumulator <= 24'd0;
            assembled_word   <= 32'd0;

            // Reset FIFO
            wr_ptr           <= {FIFO_DEPTH_BITS{1'b0}};
            rd_ptr           <= {FIFO_DEPTH_BITS{1'b0}};
            fifo_count       <= {(FIFO_DEPTH_BITS+1){1'b0}};

            // Reset AXI interface
            m_axis_tvalid    <= 1'b0;
        end else begin
            //==================================================================
            // INPUT BYTE COLLECTION AND FIFO WRITE
            //==================================================================
            // Data is only collected from data_pins if the FIFO is not full.
            // If the FIFO is full, input data is effectively dropped.
            if (!fifo_full) begin
                // Advance byte counter
                byte_counter <= byte_counter + 1;

                // Collect bytes into accumulator
                case (byte_counter)
                    2'd0: data_accumulator[7:0]   <= data_pins;
                    2'd1: data_accumulator[15:8]  <= data_pins;
                    2'd2: data_accumulator[23:16] <= data_pins;
                    2'd3: begin
                        // Assemble complete 32-bit word
                        assembled_word <= {data_pins, data_accumulator};
                        // Write to FIFO
                        fifo_mem[wr_ptr] <= {data_pins, data_accumulator};
                    end
                endcase
            end

            //==================================================================
            // FIFO POINTER MANAGEMENT
            //==================================================================
            if (do_write) begin
                // Proper wraparound for write pointer
                if (wr_ptr == FIFO_DEPTH - 1)
                    wr_ptr <= {FIFO_DEPTH_BITS{1'b0}};
                else
                    wr_ptr <= wr_ptr + 1;
            end

            if (do_read) begin
                // Proper wraparound for read pointer
                if (rd_ptr == FIFO_DEPTH - 1)
                    rd_ptr <= {FIFO_DEPTH_BITS{1'b0}};
                else
                    rd_ptr <= rd_ptr + 1;
            end

            // Update FIFO count
            fifo_count <= next_fifo_count;

            //==================================================================
            // SIMPLIFIED TVALID LOGIC
            //==================================================================
            // TVALID should be high when there's data available to read.
            // We look at the next_fifo_count to ensure TVALID is asserted
            // in the same cycle a word is written into a previously empty FIFO.
            if (next_fifo_count > 0) begin
                m_axis_tvalid <= 1'b1;
            end else begin
                m_axis_tvalid <= 1'b0;
            end
        end
    end

    //==================================================================
    // ASSERTIONS FOR DEBUGGING (synthesis pragma to remove in production)
    //==================================================================
    // synthesis translate_off
    always @(posedge aclk) begin
        if (aresetn) begin
            // Check for FIFO overflow
            // This should not be possible due to the 'if (!fifo_full)' guard
            if (fifo_count > FIFO_DEPTH) begin
                $error("FIFO overflow detected! Count: %d, Depth: %d", fifo_count, FIFO_DEPTH);
            end

            // Check for underflow
            if (do_read && fifo_empty) begin
                $error("Attempting to read from empty FIFO!");
            end

            // Check for write to full FIFO
            // This check is for the internal signal, not an external write attempt
            if (do_write && fifo_full) begin
                $error("Attempting to write to full FIFO!");
            end
        end
    end
    // synthesis translate_on

endmodule

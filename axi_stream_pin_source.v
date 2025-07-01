// SPDX-License-Identifier: MIT
/*
 * AXI4-Stream source for testing the PolarFire SoC DMA.
 *
 * v18: Corrected logic to prevent data loss and stalls by implementing
 * proper input back-pressure.
 *
 * Changes from v17:
 * - Removed 'word_ready_to_write' flag in favor of a direct handshake.
 * - The byte collection logic is now stalled if the FIFO is full and
 * a new word is ready to be written. This is handled by the
 * 'input_ready' wire.
 * - This ensures that the input 'data_pins' are only consumed when there
 * is space, preventing data loss and creating a robust stream source.
 */
module axi_stream_source (
    input  wire        aclk,
    input  wire        aresetn,

    // Data Input
    input  wire [7:0]  data_pins,

    // AXI4-Stream Master Interface (Output)
    output reg         m_axis_tvalid,
    output wire [31:0] m_axis_tdata,
    output wire        m_axis_tlast,
    output wire [1:0]  m_axis_tdest,
    output wire [3:0]  m_axis_tkeep,
    output wire [3:0]  m_axis_tstrb,
    output wire [7:0]  m_axis_tid,
    input  wire        m_axis_tready
);

    // --- Parameters ---
    localparam FIFO_DEPTH_BITS = 4;
    localparam FIFO_DEPTH      = 1 << FIFO_DEPTH_BITS;

    // --- FIFO Registers and Wires ---
    reg  [31:0] fifo_mem [0:FIFO_DEPTH-1];
    reg  [FIFO_DEPTH_BITS-1:0] wr_ptr;
    reg  [FIFO_DEPTH_BITS-1:0] rd_ptr;
    reg  [FIFO_DEPTH_BITS:0]   fifo_count;

    wire fifo_full  = (fifo_count == FIFO_DEPTH);
    wire fifo_empty = (fifo_count == 0);

    // --- Data Collection and State Registers ---
    reg [1:0]  byte_counter;
    reg [23:0] data_accumulator;

    // --- Control signals ---
    // The input is ready if we are not on the last byte of a word, OR if the FIFO has space.
    // This correctly applies back-pressure to the data collection process.
    wire input_ready = (byte_counter != 2'd3) || !fifo_full;

    wire do_write = (byte_counter == 2'd3) && input_ready;
    wire do_read  = m_axis_tvalid && m_axis_tready;

    // --- Tie off unused/static signals ---
    assign m_axis_tdest = 2'b00;
    assign m_axis_tkeep = 4'b1111;
    assign m_axis_tstrb = 4'b1111;
    assign m_axis_tid   = 8'h00;
    assign m_axis_tlast = 1'b0; // Never assert TLAST for a continuous stream

    // --- Output Assignments ---
    assign m_axis_tdata = fifo_mem[rd_ptr];

    // --- Main Logic ---

    // Input Byte Collection
    always @(posedge aclk) begin
        if (!aresetn) begin
            byte_counter     <= 2'd0;
            data_accumulator <= 24'd0;
        end else begin
            // Only consume a byte and advance if the module is ready.
            if (input_ready) begin
                byte_counter <= byte_counter + 1;
                case (byte_counter)
                    2'd0: data_accumulator[7:0]   <= data_pins;
                    2'd1: data_accumulator[15:8]  <= data_pins;
                    2'd2: data_accumulator[23:16] <= data_pins;
                    // On the 4th byte, the full word is written by the FIFO logic.
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
                fifo_mem[wr_ptr] <= {data_pins, data_accumulator};
                wr_ptr <= wr_ptr + 1;
            end
        end
    end

    // FIFO Read and AXI TVALID Logic
    always @(posedge aclk) begin
        if (!aresetn) begin
            rd_ptr <= {FIFO_DEPTH_BITS{1'b0}};
            m_axis_tvalid <= 1'b0;
        end else begin
            if (do_read) begin
                rd_ptr <= rd_ptr + 1;
            end
            // m_axis_tvalid is asserted whenever the FIFO is not empty
            m_axis_tvalid <= !fifo_empty;
        end
    end

    // FIFO Count Management
    always @(posedge aclk) begin
        if (!aresetn) begin
            fifo_count <= {(FIFO_DEPTH_BITS+1){1'b0}};
        end else begin
            if (do_write && !do_read) begin
                fifo_count <= fifo_count + 1;
            end else if (!do_write && do_read) begin
                fifo_count <= fifo_count - 1;
            end
        end
    end

    // Assertions for debugging
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
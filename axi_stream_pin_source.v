// SPDX-License-Identifier: MIT
/*
 * AXI4-Stream source for testing the PolarFire SoC DMA.
 *
 * v17: Corrected logic to prevent permanent stalling.
 * Changes from v16:
 * - Decoupled the byte collection logic from the FIFO status. The module
 * now continuously assembles a 32-bit word from the input pins.
 * - Added a 'word_ready_to_write' flag that is set when a word is assembled.
 * - A write to the FIFO is now triggered only when this flag is set AND
 * the FIFO is not full. This prevents the byte counter from freezing
 * if the FIFO fills up, which was the cause of the permanent stall.
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
    localparam FIFO_DEPTH_BITS = 4;                // FIFO depth (2^4 = 16 words)
    localparam FIFO_DEPTH      = 1 << FIFO_DEPTH_BITS;

    // --- FIFO Registers and Wires ---
    reg  [31:0] fifo_mem [0:FIFO_DEPTH-1];
    reg  [FIFO_DEPTH_BITS-1:0] wr_ptr;
    reg  [FIFO_DEPTH_BITS-1:0] rd_ptr;
    reg  [FIFO_DEPTH_BITS:0]   fifo_count; // One extra bit for full/empty distinction

    // FIFO status signals (internal)
    wire fifo_full  = (fifo_count == FIFO_DEPTH);
    wire fifo_empty = (fifo_count == 0);

    // --- Data Collection and State Registers ---
    reg [1:0]  byte_counter;
    reg [23:0] data_accumulator;
    reg [31:0] assembled_word;
    reg        word_ready_to_write; // NEW: Flag to signal a completed word

    // --- Control signals ---
    // A write happens when a new word is ready AND the FIFO has space.
    wire do_write = word_ready_to_write && !fifo_full;
    wire do_read  = m_axis_tvalid && m_axis_tready;

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
            byte_counter        <= 2'd0;
            data_accumulator    <= 24'd0;
            assembled_word      <= 32'd0;
            word_ready_to_write <= 1'b0;

            // Reset FIFO
            wr_ptr              <= {FIFO_DEPTH_BITS{1'b0}};
            rd_ptr              <= {FIFO_DEPTH_BITS{1'b0}};
            fifo_count          <= {(FIFO_DEPTH_BITS+1){1'b0}};

            // Reset AXI interface
            m_axis_tvalid       <= 1'b0;
        end else begin
            //==================================================================
            // INPUT BYTE COLLECTION (Runs continuously)
            //==================================================================
            byte_counter <= byte_counter + 1;
            case (byte_counter)
                2'd0: data_accumulator[7:0]   <= data_pins;
                2'd1: data_accumulator[15:8]  <= data_pins;
                2'd2: data_accumulator[23:16] <= data_pins;
                2'd3: assembled_word          <= {data_pins, data_accumulator};
            endcase

            //==================================================================
            // WORD READY FLAG LOGIC (NEW)
            //==================================================================
            // Set the flag for one cycle when a word is assembled.
            // Clear it after it has been successfully written to the FIFO.
            if (byte_counter == 2'd3) begin
                word_ready_to_write <= 1'b1;
            end else if (do_write) begin // A successful write consumes the flag
                word_ready_to_write <= 1'b0;
            end

            //==================================================================
            // FIFO WRITE LOGIC
            //==================================================================
            if (do_write) begin
                fifo_mem[wr_ptr] <= assembled_word;
                if (wr_ptr == FIFO_DEPTH - 1)
                    wr_ptr <= {FIFO_DEPTH_BITS{1'b0}};
                else
                    wr_ptr <= wr_ptr + 1;
            end

            //==================================================================
            // FIFO READ AND COUNT MANAGEMENT
            //==================================================================
            if (do_read) begin
                if (rd_ptr == FIFO_DEPTH - 1)
                    rd_ptr <= {FIFO_DEPTH_BITS{1'b0}};
                else
                    rd_ptr <= rd_ptr + 1;
            end
            
            // Update FIFO count based on write/read operations
            if (do_write && !do_read) begin
                fifo_count <= fifo_count + 1;
            end else if (!do_write && do_read) begin
                fifo_count <= fifo_count - 1;
            end

            //==================================================================
            // AXI TVALID LOGIC
            //==================================================================
            // TVALID is high whenever the FIFO is not empty.
            m_axis_tvalid <= !fifo_empty;

        end
    end

    //==================================================================
    // ASSERTIONS FOR DEBUGGING (synthesis pragma to remove in production)
    //==================================================================
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
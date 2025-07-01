//-----------------------------------------------------------------------------
// DUT: axi_stream_source (v22 - Final Robust Version)
//-----------------------------------------------------------------------------
`timescale 1ns / 1ps
module axi_stream_source (
    input  wire        aclk,
    input  wire        aresetn,
    input  wire [7:0]  data_pins,
    output wire        m_axis_tvalid,
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
    wire do_read = m_axis_tready && !fifo_empty;

    // --- Output Assignments ---
    assign m_axis_tvalid = !fifo_empty;
    assign m_axis_tdata  = fifo_mem[rd_ptr];
    assign m_axis_tdest = 2'b00;
    assign m_axis_tkeep = 4'b1111;
    assign m_axis_tstrb = 4'b1111;
    assign m_axis_tid   = 8'h00;
    assign m_axis_tlast = 1'b0;

    // Input Byte Collection Logic
    always @(posedge aclk) begin
        if (!aresetn) begin
            byte_counter     <= 2'd0;
            data_accumulator <= 24'd0;
        end else begin
            // Advance the counter if the FIFO is ready for the word,
            // or if the word is not yet complete.
            if (do_write || !byte_collector_valid) begin
                byte_counter <= byte_counter + 1;
                case (byte_counter)
                    2'd0: data_accumulator[7:0]   <= data_pins;
                    2'd1: data_accumulator[15:8]  <= data_pins;
                    2'd2: data_accumulator[23:16] <= data_pins;
                    // On byte 3, the full word is assembled and written by the FIFO logic
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

    // FIFO Read Logic
    always @(posedge aclk) begin
        if (!aresetn) begin
            rd_ptr <= {FIFO_DEPTH_BITS{1'b0}};
        end else begin
            if (do_read) begin
                rd_ptr <= rd_ptr + 1;
            end
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

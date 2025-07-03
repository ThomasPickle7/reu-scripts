`timescale 1ns / 1ps

////////////////////////////////////////////////////////////////////////////////
//
// AXI4-Stream Continuous Source
//
// Description:
// This module generates a continuous AXI4-Stream from an external data
// source via a FIFO. It is designed to stream indefinitely without sending
// packets, meaning TLAST is never asserted. This is suitable for continuous
// data applications like video or ADC data.
//
////////////////////////////////////////////////////////////////////////////////

module axi_continuous_stream_source (
    // Clock and Reset
    input  wire                   aclk,
    input  wire                   aresetn,

    // Data Input
    input  wire [7:0]             data_pins,

    // AXI4-Stream Master Interface
    output reg                    m_axis_tvalid,
    output reg  [31:0]            m_axis_tdata,
    output wire                   m_axis_tlast, // Now a wire, tied to 0
    output wire [1:0]             m_axis_tdest,
    output wire [3:0]             m_axis_tkeep,
    output wire [3:0]             m_axis_tstrb,
    output wire [7:0]             m_axis_tid,
    input  wire                   m_axis_tready
);

    // --- Parameters ---
    localparam FIFO_DEPTH_BITS = 4;
    localparam FIFO_DEPTH      = 1 << FIFO_DEPTH_BITS;

    // --- Internal FIFO Logic ---
    reg  [31:0] fifo_mem [0:FIFO_DEPTH-1];
    reg  [FIFO_DEPTH_BITS-1:0] wr_ptr;
    reg  [FIFO_DEPTH_BITS-1:0] rd_ptr;
    reg  [FIFO_DEPTH_BITS:0]   fifo_count;
    wire fifo_full  = (fifo_count == FIFO_DEPTH);
    wire fifo_empty = (fifo_count == 0);

    // --- Data Collection Logic ---
    reg [1:0]  byte_counter;
    reg [23:0] data_accumulator;
    wire       byte_collector_valid = (byte_counter == 2'd3);

    // --- Handshake ---
    wire do_write = byte_collector_valid && !fifo_full;
    wire do_read  = m_axis_tvalid && m_axis_tready;

    // --- Control Path FSM ---
    localparam S_IDLE      = 1'b0;
    localparam S_SEND_DATA = 1'b1;
    reg  state, next_state;

    // --- Fixed AXI Signal Assignments ---
    assign m_axis_tlast  = 1'b0; // Never the last transfer
    assign m_axis_tdest  = 2'b00;
    assign m_axis_tkeep  = 4'b1111;
    assign m_axis_tstrb  = 4'b1111;
    assign m_axis_tid    = 8'h00;


    //================================================================
    // BLOCK 1: Input Byte Collection & FIFO Management
    //================================================================
    always @(posedge aclk) begin
        if (!aresetn) begin
            byte_counter     <= 2'd0;
            data_accumulator <= 24'd0;
            wr_ptr           <= 0;
            fifo_count       <= 0;
        end else begin
            // Manage FIFO count based on writes and reads
            if (do_write && !do_read)
                fifo_count <= fifo_count + 1;
            else if (!do_write && do_read)
                fifo_count <= fifo_count - 1;

            // Collect input bytes into words
            if (do_write || !byte_collector_valid) begin
                byte_counter <= byte_counter + 1;
                case (byte_counter)
                    2'd0: data_accumulator[7:0]   <= data_pins;
                    2'd1: data_accumulator[15:8]  <= data_pins;
                    2'd2: data_accumulator[23:16] <= data_pins;
                    default: ;
                endcase
            end
            if (do_write) begin
                fifo_mem[wr_ptr] <= {data_pins, data_accumulator};
                wr_ptr <= wr_ptr + 1;
            end
        end
    end

    //================================================================
    // BLOCK 2: CONTROL & DATA PATH
    //================================================================
    always @(posedge aclk) begin
        if (!aresetn) begin
            state <= S_IDLE;
            m_axis_tvalid <= 1'b0;
            m_axis_tdata  <= 32'd0;
            rd_ptr        <= 0;
        end else begin
            state <= next_state;
            // Manage AXI outputs based on FSM state
            case(state)
                S_IDLE: begin
                    if (!fifo_empty) begin
                        m_axis_tvalid <= 1'b1;
                        m_axis_tdata  <= fifo_mem[rd_ptr];
                    end else begin
                        m_axis_tvalid <= 1'b0;
                    end
                end
                S_SEND_DATA: begin
                    m_axis_tvalid <= 1'b1; // Keep valid asserted
                    if (do_read) begin
                        m_axis_tdata <= fifo_mem[rd_ptr + 1];
                        rd_ptr       <= rd_ptr + 1;
                    end
                end
            endcase
        end
    end

    always @* begin
        next_state = state;
        case(state)
            S_IDLE:
                if (!fifo_empty)
                    next_state = S_SEND_DATA;
            S_SEND_DATA:
                if (fifo_empty && !do_read) // Go idle if FIFO runs out
                    next_state = S_IDLE;
        endcase
    end

endmodule

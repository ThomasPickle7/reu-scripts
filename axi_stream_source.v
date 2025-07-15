// ****************************************************************************/
// Description:
// This module implements an AXI4-Stream Master with a memory-mapped AXI4-Lite
// slave interface for control. This version features a robust, state-machine-
// based AXI4-Lite slave with combinational READY signals for correct write
// and read operations.
// ****************************************************************************/
`timescale 1ns / 1ps
module AXI4StreamMaster (
    // System Signals
    clock,
    resetn,

    // AXI4-Stream Master Interface (Output)
    TDATA,
    TLAST,
    TID,
    TDEST,
    TVALID,
    TREADY,

    // AXI4-Lite Slave Interface (Input for Control)
    S_AXI_AWADDR,
    S_AXI_AWPROT,
    S_AXI_AWVALID,
    S_AXI_AWREADY,
    S_AXI_WDATA,
    S_AXI_WSTRB,
    S_AXI_WVALID,
    S_AXI_WREADY,
    S_AXI_BRESP,
    S_AXI_BVALID,
    S_AXI_BREADY,
    S_AXI_ARADDR,
    S_AXI_ARPROT,
    S_AXI_ARVALID,
    S_AXI_ARREADY,
    S_AXI_RDATA,
    S_AXI_RRESP,
    S_AXI_RVALID,
    S_AXI_RREADY
);

////////////////////////////////////////////////////////////////////////////////
// Parameters
////////////////////////////////////////////////////////////////////////////////
parameter STREAM_DATA_WIDTH  = 32;
parameter STREAM_ID_WIDTH    = 2;
localparam STREAM_DATA_BYTES = (STREAM_DATA_WIDTH/8);
parameter C_S_AXI_DATA_WIDTH = 32;
parameter C_S_AXI_ADDR_WIDTH = 5;
localparam ADDR_CONTROL_REG   = 5'h00;
localparam ADDR_STATUS_REG    = 5'h04;
localparam ADDR_NUM_BYTES_REG = 5'h10;
localparam ADDR_DEST_REG      = 5'h14;

////////////////////////////////////////////////////////////////////////////////
// Port List
////////////////////////////////////////////////////////////////////////////////
input  logic                           clock;
input  logic                           resetn;
output logic [STREAM_DATA_WIDTH-1:0]   TDATA;
output logic                           TLAST;
output logic [STREAM_ID_WIDTH-1:0]     TID;
output logic [1:0]                     TDEST;
output logic                           TVALID;
input  logic                           TREADY;
input  logic [C_S_AXI_ADDR_WIDTH-1:0]  S_AXI_AWADDR;
input  logic [2:0]                     S_AXI_AWPROT;
input  logic                           S_AXI_AWVALID;
output logic                           S_AXI_AWREADY;
input  logic [C_S_AXI_DATA_WIDTH-1:0]  S_AXI_WDATA;
input  logic [C_S_AXI_DATA_WIDTH/8-1:0]S_AXI_WSTRB;
input  logic                           S_AXI_WVALID;
output logic                           S_AXI_WREADY;
output logic [1:0]                     S_AXI_BRESP;
output logic                           S_AXI_BVALID;
input  logic                           S_AXI_BREADY;
input  logic [C_S_AXI_ADDR_WIDTH-1:0]  S_AXI_ARADDR;
input  logic [2:0]                     S_AXI_ARPROT;
input  logic                           S_AXI_ARVALID;
output logic                           S_AXI_ARREADY;
output logic [C_S_AXI_DATA_WIDTH-1:0]  S_AXI_RDATA;
output logic [1:0]                     S_AXI_RRESP;
output logic                           S_AXI_RVALID;
input  logic                           S_AXI_RREADY;

////////////////////////////////////////////////////////////////////////////////
// AXI4-Lite Register Logic -- FINAL CORRECTED VERSION
////////////////////////////////////////////////////////////////////////////////

// --- Internal Signals ---
logic [C_S_AXI_ADDR_WIDTH-1:0] axi_awaddr_reg;
logic [C_S_AXI_ADDR_WIDTH-1:0] axi_araddr_reg;

logic S_AXI_BVALID_i;
logic S_AXI_RVALID_i;
logic [C_S_AXI_DATA_WIDTH-1:0] S_AXI_RDATA_i;

logic [C_S_AXI_DATA_WIDTH-1:0] reg_control;
logic [C_S_AXI_DATA_WIDTH-1:0] reg_status;
logic [C_S_AXI_DATA_WIDTH-1:0] reg_num_bytes;
logic [C_S_AXI_DATA_WIDTH-1:0] reg_dest;
logic                          start_pulse;

// Handshake signals
wire axi_write_fire = S_AXI_WVALID && S_AXI_WREADY;
wire axi_aw_fire    = S_AXI_AWVALID && S_AXI_AWREADY;
wire axi_ar_fire    = S_AXI_ARVALID && S_AXI_ARREADY;
wire axi_r_fire     = S_AXI_RVALID && S_AXI_RREADY;
wire axi_b_fire     = S_AXI_BVALID && S_AXI_BREADY;

// --- AXI Write Channel Logic ---
typedef enum logic [1:0] {
    WRITE_IDLE,
    WRITE_DATA,
    WRITE_RESP
} write_state_t;

write_state_t write_state;

// Write State Machine (Registered)
always_ff @(posedge clock) begin
    if (!resetn) begin
        write_state <= WRITE_IDLE;
    end else begin
        case (write_state)
            WRITE_IDLE: if (axi_aw_fire) write_state <= WRITE_DATA;
            WRITE_DATA: if (axi_write_fire) write_state <= WRITE_RESP;
            WRITE_RESP: if (axi_b_fire) write_state <= WRITE_IDLE;
        endcase
    end
end

// Combinational READY signals based on current state
assign S_AXI_AWREADY = (write_state == WRITE_IDLE);
assign S_AXI_WREADY  = (write_state == WRITE_DATA);

// Registered BVALID signal
always_ff @(posedge clock) begin
    if (!resetn) begin
        S_AXI_BVALID_i <= 1'b0;
    end else begin
        if (axi_write_fire) S_AXI_BVALID_i <= 1'b1;
        else if (axi_b_fire) S_AXI_BVALID_i <= 1'b0;
    end
end

// Latch write address
always_ff @(posedge clock) begin
    if (axi_aw_fire) begin
        axi_awaddr_reg <= S_AXI_AWADDR;
    end
end

// Register write logic
always_ff @(posedge clock) begin
    if (!resetn) begin
        reg_control   <= '0;
        reg_num_bytes <= '0;
        reg_dest      <= '0;
    end else begin
        if (axi_write_fire) begin
            case (axi_awaddr_reg)
                ADDR_CONTROL_REG:   reg_control   <= S_AXI_WDATA;
                ADDR_NUM_BYTES_REG: reg_num_bytes <= S_AXI_WDATA;
                ADDR_DEST_REG:      reg_dest      <= S_AXI_WDATA;
                default:;
            endcase
        end
    end
end

// --- AXI Read Channel Logic ---
typedef enum logic {
    READ_IDLE,
    READ_DATA
} read_state_t;

read_state_t read_state;

// Read State Machine (Registered)
always_ff @(posedge clock) begin
    if (!resetn) begin
        read_state <= READ_IDLE;
    end else begin
        case (read_state)
            READ_IDLE: if (axi_ar_fire) read_state <= READ_DATA;
            READ_DATA: if (axi_r_fire) read_state <= READ_IDLE;
        endcase
    end
end

// Combinational ARREADY signal
assign S_AXI_ARREADY = (read_state == READ_IDLE);

// Registered RVALID signal
always_ff @(posedge clock) begin
    if (!resetn) begin
        S_AXI_RVALID_i <= 1'b0;
    end else begin
        if (axi_ar_fire) S_AXI_RVALID_i <= 1'b1;
        else if (axi_r_fire) S_AXI_RVALID_i <= 1'b0;
    end
end

// Read data multiplexer logic
always_ff @(posedge clock) begin
    // Latch the read address and select data on the address handshake
    if (axi_ar_fire) begin
        case (S_AXI_ARADDR)
            ADDR_CONTROL_REG:   S_AXI_RDATA_i <= reg_control;
            ADDR_STATUS_REG:    S_AXI_RDATA_i <= reg_status;
            ADDR_NUM_BYTES_REG: S_AXI_RDATA_i <= reg_num_bytes;
            ADDR_DEST_REG:      S_AXI_RDATA_i <= reg_dest;
            default:            S_AXI_RDATA_i <= '0;
        endcase
    end
end

// --- Start Pulse Generation ---
assign start_pulse = axi_write_fire && (axi_awaddr_reg == ADDR_CONTROL_REG) && S_AXI_WDATA[0];

// --- Assign Final Outputs ---
assign S_AXI_BVALID  = S_AXI_BVALID_i;
assign S_AXI_BRESP   = 2'b00; // OKAY
assign S_AXI_RVALID  = S_AXI_RVALID_i;
assign S_AXI_RDATA   = S_AXI_RDATA_i;
assign S_AXI_RRESP   = 2'b00; // OKAY


////////////////////////////////////////////////////////////////////////////////
// Core Stream Generator Logic -- (No Changes Needed Here)
////////////////////////////////////////////////////////////////////////////////
typedef enum logic [1:0] {
    IDLE,
    STREAM_DATA,
    STREAM_LAST
} state_t;

state_t current_state, next_state;

logic [31:0] data_reg;
logic [23:0] transfer_counter;

wire [23:0] transfers_in_packet = reg_num_bytes[23:0] / STREAM_DATA_BYTES;
wire transfer_fire = TVALID & TREADY;

always_ff @(posedge clock or negedge resetn) begin
    if (!resetn) current_state <= IDLE;
    else current_state <= next_state;
end

always_comb begin
    next_state = current_state;
    case (current_state)
        IDLE: if (start_pulse) begin
            if (transfers_in_packet > 1) next_state = STREAM_DATA;
            else if (transfers_in_packet == 1) next_state = STREAM_LAST;
        end
        STREAM_DATA: if (transfer_fire && (transfer_counter == transfers_in_packet - 2)) begin
            next_state = STREAM_LAST;
        end
        STREAM_LAST: if (transfer_fire) begin
            next_state = IDLE;
        end
    endcase
end

always_ff @(posedge clock or negedge resetn) begin
    if (!resetn) begin
        data_reg <= 32'b0;
        transfer_counter <= '0;
        TDATA <= 32'b0;
        TVALID <= 1'b0;
        TLAST <= 1'b0;
        TDEST <= 2'b0;
        reg_status <= '0;
    end else begin
        if (start_pulse) begin
            TDEST <= reg_dest[1:0];
            data_reg <= 32'b0;
            transfer_counter <= '0;
        end
        TVALID <= (current_state == STREAM_DATA || current_state == STREAM_LAST);
        TLAST  <= (current_state == STREAM_LAST);
        TDATA  <= data_reg;
        reg_status[0] <= (current_state != IDLE) || start_pulse;
        if (transfer_fire) begin
            data_reg <= data_reg + 1;
            if (transfer_counter == transfers_in_packet - 1) transfer_counter <= '0;
            else transfer_counter <= transfer_counter + 1;
        end
    end
end

assign TID = '0;

endmodule

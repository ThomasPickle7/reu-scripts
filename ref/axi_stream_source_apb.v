// AXI4StreamMaster_APB.v -- CORRECTED VERSION

`timescale 1ns / 1ps
module AXI4StreamMaster_APB (
    // System Signals
    PCLK,
    PRESETn,

    // AXI4-Stream Master Interface (Output)
    TDATA,
    TLAST,
    TID,
    TDEST,
    TVALID,
    TREADY,

    // APB Slave Interface (Input for Control)
    PSEL,
    PENABLE,
    PWRITE,
    PADDR,
    PWDATA,
    PRDATA,
    PREADY,
    PSLVERR
);

////////////////////////////////////////////////////////////////////////////////
// Parameters & Ports (No changes here)
////////////////////////////////////////////////////////////////////////////////
parameter STREAM_DATA_WIDTH = 32;
parameter STREAM_ID_WIDTH   = 2;
localparam STREAM_DATA_BYTES = (STREAM_DATA_WIDTH/8);
parameter C_APB_DATA_WIDTH = 32;
parameter C_APB_ADDR_WIDTH = 5;
localparam ADDR_CONTROL_REG   = 5'h00;
localparam ADDR_STATUS_REG    = 5'h04;
localparam ADDR_NUM_BYTES_REG = 5'h10;
localparam ADDR_DEST_REG      = 5'h14;

input  logic                        PCLK;
input  logic                        PRESETn;
output logic [STREAM_DATA_WIDTH-1:0] TDATA;
output logic                         TLAST;
output logic [STREAM_ID_WIDTH-1:0]   TID;
output logic [1:0]                   TDEST;
output logic                         TVALID;
input  logic                         TREADY;
input  logic                        PSEL;
input  logic                        PENABLE;
input  logic                        PWRITE;
input  logic [C_APB_ADDR_WIDTH-1:0] PADDR;
input  logic [C_APB_DATA_WIDTH-1:0] PWDATA;
output logic [C_APB_DATA_WIDTH-1:0] PRDATA;
output logic                        PREADY;
output logic                        PSLVERR;


////////////////////////////////////////////////////////////////////////////////
// APB Control Interface Logic (No changes here)
////////////////////////////////////////////////////////////////////////////////
logic [C_APB_DATA_WIDTH-1:0] reg_control;
logic [C_APB_DATA_WIDTH-1:0] reg_status;
logic [C_APB_DATA_WIDTH-1:0] reg_num_bytes;
logic [C_APB_DATA_WIDTH-1:0] reg_dest;
logic                        start_pulse;
wire clock  = PCLK;
wire resetn = PRESETn;
wire apb_access = PSEL && PENABLE;

always_ff @(posedge clock or negedge resetn) begin
    if (!resetn) begin reg_control <= '0; reg_num_bytes <= '0; reg_dest <= '0; end
    else if (apb_access && PWRITE) begin
        case (PADDR)
            ADDR_CONTROL_REG:   reg_control   <= PWDATA;
            ADDR_NUM_BYTES_REG: reg_num_bytes <= PWDATA;
            ADDR_DEST_REG:      reg_dest      <= PWDATA;
            default:;
        endcase
    end
end
always_comb begin
    case(PADDR)
        ADDR_CONTROL_REG:   PRDATA = reg_control;
        ADDR_STATUS_REG:    PRDATA = reg_status;
        ADDR_NUM_BYTES_REG: PRDATA = reg_num_bytes;
        ADDR_DEST_REG:      PRDATA = reg_dest;
        default:            PRDATA = '0;
    endcase
end
assign PREADY = 1'b1;
assign PSLVERR = 1'b0;
assign start_pulse = apb_access && PWRITE && (PADDR == ADDR_CONTROL_REG) && PWDATA[0];


////////////////////////////////////////////////////////////////////////////////
// Core Stream Generator Logic -- (FIXES APPLIED HERE)
////////////////////////////////////////////////////////////////////////////////
typedef enum logic [1:0] { IDLE, STREAM_DATA, STREAM_LAST } state_t;
state_t current_state, next_state;

logic [31:0] data_reg;
logic [23:0] transfer_counter;

wire [23:0] transfers_in_packet = reg_num_bytes[23:0] / STREAM_DATA_BYTES;
wire transfer_fire = TVALID & TREADY;

// --- FIX #1: TDATA is now a combinational assignment from data_reg ---
// This ensures the data is aligned with TVALID on the same clock cycle.
assign TDATA = data_reg;

// --- FIX #2: TLAST is now a combinational assignment from the FSM state ---
// This ensures TLAST is asserted on the correct cycle.
assign TLAST = (current_state == STREAM_LAST);

// FSM State Register
always_ff @(posedge clock or negedge resetn) begin
    if (!resetn) current_state <= IDLE;
    else current_state <= next_state;
end

// FSM Next-State Logic (No changes here)
always_comb begin
    next_state = current_state;
    case (current_state)
        IDLE: if (start_pulse) begin
            if (transfers_in_packet > 1) next_state = STREAM_DATA;
            else if (transfers_in_packet == 1) next_state = STREAM_LAST;
            else next_state = IDLE; // Handle zero-length packet
        end
        STREAM_DATA: if (transfer_fire && (transfer_counter == transfers_in_packet - 2)) begin
            next_state = STREAM_LAST;
        end
        STREAM_LAST: if (transfer_fire) begin
            next_state = IDLE;
        end
    endcase
end

// Datapath and Control Register Updates
always_ff @(posedge clock or negedge resetn) begin
    if (!resetn) begin
        data_reg <= 32'b0;
        transfer_counter <= '0;
        TVALID <= 1'b0;
        TDEST <= 2'b0;
        reg_status <= '0;
    end else begin
        if (start_pulse) begin
            TDEST <= reg_dest[1:0];
            data_reg <= 32'b0;
            transfer_counter <= '0;
        end
        
        TVALID <= (current_state == STREAM_DATA || current_state == STREAM_LAST);
        // TLAST and TDATA removed from this block

        reg_status[0] <= (current_state != IDLE) || start_pulse;

        if (transfer_fire) begin
            data_reg <= data_reg + 1;
            if (transfer_counter == transfers_in_packet - 1) begin
                transfer_counter <= '0;
            end else begin
                transfer_counter <= transfer_counter + 1;
            end
        end
    end
end

assign TID = '0;

endmodule
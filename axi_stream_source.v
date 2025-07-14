// ****************************************************************************/
// Description:
// This module implements an AXI4-Stream Master with a memory-mapped AXI4-Lite
// slave interface for control. It generates AXI4-Stream packets when commanded
// via its control registers. This version is synthesizable and can be
// integrated into a processor system.
// ****************************************************************************/

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
// AXI Stream Parameters
parameter STREAM_DATA_WIDTH  = 32;
parameter STREAM_ID_WIDTH    = 2;
localparam STREAM_DATA_BYTES = (STREAM_DATA_WIDTH/8);

// AXI Lite Parameters
parameter C_S_AXI_DATA_WIDTH = 32;
parameter C_S_AXI_ADDR_WIDTH = 5; // 2^5 = 32 addresses, enough for our registers

// Register Address Map
localparam ADDR_CONTROL_REG = 5'h00;
localparam ADDR_STATUS_REG  = 5'h04;
localparam ADDR_NUM_BYTES_REG = 5'h10;
localparam ADDR_DEST_REG    = 5'h14;


////////////////////////////////////////////////////////////////////////////////
// Port List
////////////////////////////////////////////////////////////////////////////////
// System Signals
input  logic                       clock;
input  logic                       resetn;

// AXI4-Stream Master Interface
output logic [STREAM_DATA_WIDTH-1:0] TDATA;
output logic                       TLAST;
output logic [STREAM_ID_WIDTH-1:0] TID;
output logic [1:0]                 TDEST;
output logic                       TVALID;
input  logic                       TREADY;

// AXI4-Lite Slave Interface
input  logic [C_S_AXI_ADDR_WIDTH-1:0] S_AXI_AWADDR;
input  logic [2:0]                    S_AXI_AWPROT;
input  logic                          S_AXI_AWVALID;
output logic                          S_AXI_AWREADY;
input  logic [C_S_AXI_DATA_WIDTH-1:0] S_AXI_WDATA;
input  logic [C_S_AXI_DATA_WIDTH/8-1:0] S_AXI_WSTRB;
input  logic                          S_AXI_WVALID;
output logic                          S_AXI_WREADY;
output logic [1:0]                    S_AXI_BRESP;
output logic                          S_AXI_BVALID;
input  logic                          S_AXI_BREADY;
input  logic [C_S_AXI_ADDR_WIDTH-1:0] S_AXI_ARADDR;
input  logic [2:0]                    S_AXI_ARPROT;
input  logic                          S_AXI_ARVALID;
output logic                          S_AXI_ARREADY;
output logic [C_S_AXI_DATA_WIDTH-1:0] S_AXI_RDATA;
output logic [1:0]                    S_AXI_RRESP;
output logic                          S_AXI_RVALID;
input  logic                          S_AXI_RREADY;


////////////////////////////////////////////////////////////////////////////////
// AXI4-Lite Register Logic
////////////////////////////////////////////////////////////////////////////////
logic [C_S_AXI_ADDR_WIDTH-1:0] axi_awaddr;
logic                          axi_awready;
logic                          axi_wready;
logic [1:0]                    axi_bresp;
logic                          axi_bvalid;
logic [C_S_AXI_ADDR_WIDTH-1:0] axi_araddr;
logic                          axi_arready;
logic [C_S_AXI_DATA_WIDTH-1:0] axi_rdata;
logic [1:0]                    axi_rresp;
logic                          axi_rvalid;

// Internal Registers
logic [C_S_AXI_DATA_WIDTH-1:0] reg_control;
logic [C_S_AXI_DATA_WIDTH-1:0] reg_status;
logic [C_S_AXI_DATA_WIDTH-1:0] reg_num_bytes;
logic [C_S_AXI_DATA_WIDTH-1:0] reg_dest;

logic                          start_pulse;

// Assign outputs
assign S_AXI_AWREADY = axi_awready;
assign S_AXI_WREADY  = axi_wready;
assign S_AXI_BRESP   = axi_bresp;
assign S_AXI_BVALID  = axi_bvalid;
assign S_AXI_ARREADY = axi_arready;
assign S_AXI_RDATA   = axi_rdata;
assign S_AXI_RRESP   = axi_rresp;
assign S_AXI_RVALID  = axi_rvalid;

// AXI Write Logic
always_ff @(posedge clock) begin
    if (!resetn) begin
        axi_awready <= 1'b0;
        axi_awaddr  <= '0;
    end else begin
        if (~axi_awready && S_AXI_AWVALID && S_AXI_WVALID) begin
            axi_awready <= 1'b1;
            axi_awaddr  <= S_AXI_AWADDR;
        end else begin
            axi_awready <= 1'b0;
        end
    end
end

always_ff @(posedge clock) begin
    if (!resetn) begin
        axi_wready <= 1'b0;
    end else begin
        if (~axi_wready && S_AXI_WVALID && S_AXI_AWVALID) begin
            axi_wready <= 1'b1;
        end else begin
            axi_wready <= 1'b0;
        end
    end
end

always_ff @(posedge clock) begin
    if (!resetn) begin
        reg_control   <= '0;
        reg_num_bytes <= '0;
        reg_dest      <= '0;
        start_pulse   <= 1'b0;
    end else begin
        start_pulse <= 1'b0; // Pulse is only one cycle
        if (axi_awready && S_AXI_AWVALID && axi_wready && S_AXI_WVALID) begin
            case (axi_awaddr)
                ADDR_CONTROL_REG: begin
                    reg_control <= S_AXI_WDATA;
                    if (S_AXI_WDATA[0]) begin // Check for start bit
                        start_pulse <= 1'b1;
                    end
                end
                ADDR_NUM_BYTES_REG: reg_num_bytes <= S_AXI_WDATA;
                ADDR_DEST_REG:      reg_dest      <= S_AXI_WDATA;
                default:;
            endcase
        end
    end
end

always_ff @(posedge clock) begin
    if (!resetn) begin
        axi_bvalid <= 1'b0;
        axi_bresp  <= 2'b0;
    end else begin
        if (axi_awready && S_AXI_AWVALID && axi_wready && S_AXI_WVALID && ~axi_bvalid) begin
            axi_bvalid <= 1'b1;
            axi_bresp  <= 2'b0; // OKAY
        end else if (S_AXI_BREADY && axi_bvalid) begin
            axi_bvalid <= 1'b0;
        end
    end
end

// AXI Read Logic
always_ff @(posedge clock) begin
    if (!resetn) begin
        axi_arready <= 1'b0;
        axi_araddr  <= '0;
    end else begin
        if (~axi_arready && S_AXI_ARVALID) begin
            axi_arready <= 1'b1;
            axi_araddr  <= S_AXI_ARADDR;
        end else begin
            axi_arready <= 1'b0;
        end
    end
end

always_ff @(posedge clock) begin
    if (!resetn) begin
        axi_rvalid <= 1'b0;
        axi_rresp  <= 2'b0;
    end else begin
        if (axi_arready && S_AXI_ARVALID && ~axi_rvalid) begin
            axi_rvalid <= 1'b1;
            axi_rresp  <= 2'b0; // OKAY
        end else if (S_AXI_RREADY && axi_rvalid) begin
            axi_rvalid <= 1'b0;
        end
    end
end

always_ff @(posedge clock) begin
    if (axi_arready && S_AXI_ARVALID) begin
        case (axi_araddr)
            ADDR_CONTROL_REG:   axi_rdata <= reg_control;
            ADDR_STATUS_REG:    axi_rdata <= reg_status;
            ADDR_NUM_BYTES_REG: axi_rdata <= reg_num_bytes;
            ADDR_DEST_REG:      axi_rdata <= reg_dest;
            default:            axi_rdata <= '0;
        endcase
    end
end


////////////////////////////////////////////////////////////////////////////////
// Core Stream Generator Logic
////////////////////////////////////////////////////////////////////////////////
typedef enum logic [1:0] {
    IDLE,
    STREAM_DATA,
    STREAM_LAST
} state_t;

state_t current_state, next_state;

logic [31:0] data_reg;
logic [23:0] transfer_counter;
logic [23:0] transfers_in_packet;

wire transfer_fire = TVALID & TREADY;

// State Register
always_ff @(posedge clock or negedge resetn) begin
    if (!resetn) begin
        current_state <= IDLE;
    end else begin
        current_state <= next_state;
    end
end

// Next State Logic
always_comb begin
    next_state = current_state;
    case (current_state)
        IDLE: begin
            if (start_pulse) begin
                if (transfers_in_packet > 1) begin
                    next_state = STREAM_DATA;
                end else if (transfers_in_packet == 1) begin
                    next_state = STREAM_LAST;
                end
            end
        end
        STREAM_DATA: begin
            if (transfer_fire && (transfer_counter == transfers_in_packet - 2)) begin
                next_state = STREAM_LAST;
            end
        end
        STREAM_LAST: begin
            if (transfer_fire) begin
                next_state = IDLE;
            end
        end
        default: begin
            next_state = IDLE;
        end
    endcase
end

// Datapath and Output Logic
always_ff @(posedge clock or negedge resetn) begin
    if (!resetn) begin
        data_reg           <= 32'b0;
        transfer_counter   <= '0;
        transfers_in_packet <= '0;
        TDATA              <= 32'b0;
        TVALID             <= 1'b0;
        TLAST              <= 1'b0;
        TDEST              <= 2'b0;
        reg_status         <= '0;
    end else begin
        // Load transaction configuration on start
        if (start_pulse) begin
            transfers_in_packet <= reg_num_bytes[23:0] / STREAM_DATA_BYTES;
            TDEST               <= reg_dest[1:0];
            data_reg            <= 32'b0; // Reset data generator for each packet
            transfer_counter    <= '0;
        end

        // Drive outputs based on state
        TVALID <= (current_state == STREAM_DATA || current_state == STREAM_LAST);
        TLAST  <= (current_state == STREAM_LAST);
        TDATA  <= data_reg;

        // Update status register
        reg_status[0] <= (current_state != IDLE);

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

// Tie off unused stream signals
assign TID   = '0;

endmodule

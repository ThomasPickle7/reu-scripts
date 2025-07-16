module AXI4StreamMaster (
    // System Signals
    clock,
    resetn,

    // AXI4-Stream Master Interface (Output)
    TDATA, TLAST, TID, TDEST, TVALID, TREADY,

    // AXI4-Lite Slave Interface
    S_AXI_AWADDR, S_AXI_AWPROT, S_AXI_AWVALID, S_AXI_AWREADY,
    S_AXI_WDATA, S_AXI_WSTRB, S_AXI_WVALID, S_AXI_WREADY,
    S_AXI_BRESP, S_AXI_BVALID, S_AXI_BREADY,
    S_AXI_ARADDR, S_AXI_ARPROT, S_AXI_ARVALID, S_AXI_ARREADY,
    S_AXI_RDATA, S_AXI_RRESP, S_AXI_RVALID, S_AXI_RREADY
);

//--- Parameters and Port Declarations ---
parameter STREAM_DATA_WIDTH  = 32;
parameter STREAM_ID_WIDTH    = 2;
parameter C_S_AXI_DATA_WIDTH = 32;
parameter C_S_AXI_ADDR_WIDTH = 5;

localparam STREAM_DATA_BYTES = (STREAM_DATA_WIDTH/8);
localparam ADDR_CONTROL_REG  = 5'h00;
localparam ADDR_STATUS_REG   = 5'h04;
localparam ADDR_NUM_BYTES_REG = 5'h10;
localparam ADDR_DEST_REG     = 5'h14;

input  logic [C_S_AXI_ADDR_WIDTH-1:0]   S_AXI_AWADDR;
input  logic [2:0]                          S_AXI_AWPROT;
input  logic                                S_AXI_AWVALID;
output logic                                S_AXI_AWREADY;
input  logic [C_S_AXI_DATA_WIDTH-1:0]   S_AXI_WDATA;
input  logic [C_S_AXI_DATA_WIDTH/8-1:0] S_AXI_WSTRB;
input  logic                                S_AXI_WVALID;
output logic                                S_AXI_WREADY;
output logic [1:0]                          S_AXI_BRESP;
output logic                                S_AXI_BVALID;
input  logic                                S_AXI_BREADY;
input  logic [C_S_AXI_ADDR_WIDTH-1:0]   S_AXI_ARADDR;
input  logic [2:0]                          S_AXI_ARPROT;
input  logic                                S_AXI_ARVALID;
output logic                                S_AXI_ARREADY;
output logic [C_S_AXI_DATA_WIDTH-1:0]   S_AXI_RDATA;
output logic [1:0]                          S_AXI_RRESP;
output logic                                S_AXI_RVALID;
input  logic                                S_AXI_RREADY;
input  logic                                clock;
input  logic                                resetn;
output logic [STREAM_DATA_WIDTH-1:0]   TDATA;
output logic                                TLAST;
output logic [STREAM_ID_WIDTH-1:0]     TID;
output logic [1:0]                          TDEST;
output logic                                TVALID;
input  logic                                TREADY;

//--- Internal Signals ---
logic [C_S_AXI_DATA_WIDTH-1:0] reg_control;
logic [C_S_AXI_DATA_WIDTH-1:0] reg_status;
logic [C_S_AXI_DATA_WIDTH-1:0] reg_num_bytes;
logic [C_S_AXI_DATA_WIDTH-1:0] reg_dest;

logic [C_S_AXI_ADDR_WIDTH-1:0]   axi_awaddr_reg;
logic                            axi_wready_reg;

logic [C_S_AXI_ADDR_WIDTH-1:0]   axi_araddr_reg;
logic [C_S_AXI_DATA_WIDTH-1:0]   axi_rdata_reg;

//--- AXI4-Lite Logic ---
assign S_AXI_BRESP = 2'b0;
assign S_AXI_RRESP = 2'b0;

// Write Logic
assign S_AXI_AWREADY = !axi_wready_reg; // Can accept address if not waiting for data
assign S_AXI_WREADY  = axi_wready_reg;  // Can accept data only after address is latched

always_ff @(posedge clock) begin
    if (!resetn) begin
        axi_awaddr_reg <= '0;
        axi_wready_reg <= 1'b0;
        S_AXI_BVALID   <= 1'b0;
        reg_control    <= '0;
        reg_num_bytes  <= '0;
        reg_dest       <= '0;
    end else begin
        // Address latching phase
        if (S_AXI_AWVALID && S_AXI_AWREADY) begin
            axi_awaddr_reg <= S_AXI_AWADDR;
            axi_wready_reg <= 1'b1;
        end else if (S_AXI_WVALID && S_AXI_WREADY) begin
            // De-assert wready once data is taken
            axi_wready_reg <= 1'b0;
        end
        
        // Data and response phase
        if (S_AXI_WVALID && S_AXI_WREADY) begin
            S_AXI_BVALID <= 1'b1;
            case (axi_awaddr_reg)
                ADDR_CONTROL_REG:   reg_control   <= S_AXI_WDATA;
                ADDR_NUM_BYTES_REG: reg_num_bytes <= S_AXI_WDATA;
                ADDR_DEST_REG:      reg_dest      <= S_AXI_WDATA;
            endcase
        end else if (S_AXI_BREADY && S_AXI_BVALID) begin
            S_AXI_BVALID <= 1'b0;
        end
    end
end

// Read Logic
assign S_AXI_ARREADY = !S_AXI_RVALID; // Ready to take new address if not sending data

always_ff @(posedge clock) begin
    if (!resetn) begin
        S_AXI_RVALID  <= 1'b0;
        axi_rdata_reg <= '0;
    end else begin
        if (S_AXI_ARVALID && S_AXI_ARREADY) begin
            S_AXI_RVALID   <= 1'b1; // Data will be valid on the next cycle
            // Latch the data to be output on the next cycle
            case(S_AXI_ARADDR)
                ADDR_CONTROL_REG:   axi_rdata_reg <= reg_control;
                ADDR_STATUS_REG:    axi_rdata_reg <= reg_status;
                ADDR_NUM_BYTES_REG: axi_rdata_reg <= reg_num_bytes;
                ADDR_DEST_REG:      axi_rdata_reg <= reg_dest;
                default:            axi_rdata_reg <= '0;
            endcase
        end else if (S_AXI_RREADY && S_AXI_RVALID) begin
            S_AXI_RVALID  <= 1'b0;
        end
    end
end
assign S_AXI_RDATA = axi_rdata_reg;


//--- Stream Generator Logic ---
logic start_transfer;
logic transfer_in_progress;
logic [31:0] data_counter;

// Capture the start signal for one cycle
always_ff @(posedge clock) begin
    if(!resetn) begin
        start_transfer <= 1'b0;
    end else begin
        // Check for a write to the control register where the LSB is 1
        if (S_AXI_WVALID && S_AXI_WREADY && (axi_awaddr_reg == ADDR_CONTROL_REG) && S_AXI_WDATA[0]) begin
            start_transfer <= 1'b1;
        end else begin
            start_transfer <= 1'b0;
        end
    end
end

// FSM for the transfer state
always_ff @(posedge clock) begin
    if (!resetn) begin
        transfer_in_progress <= 1'b0;
    end else begin
        if (start_transfer) begin
            transfer_in_progress <= 1'b1;
        end else if (TLAST && TREADY) begin
            transfer_in_progress <= 1'b0;
        end
    end
end

// Data counter logic
always_ff @(posedge clock) begin
    if (!resetn) begin
        data_counter <= '0;
    // Reset counter when a new transfer starts
    end else if (start_transfer) begin
        data_counter <= '0;
    // Increment when data is successfully transferred
    end else if (transfer_in_progress && TVALID && TREADY) begin
        data_counter <= data_counter + 1;
    end
end

assign TVALID = transfer_in_progress;
assign TDATA  = data_counter; // Simple incrementing data pattern
assign TLAST  = (transfer_in_progress) && (data_counter == (reg_num_bytes / STREAM_DATA_BYTES) - 1'b1);
assign TDEST  = reg_dest[1:0];
assign TID    = '0; // TID not used in this example

// Status register is now purely combinational, driven by a single source.
always_comb begin
    reg_status = '0; // Default all bits to 0
    reg_status[0] = transfer_in_progress; // Bit 0 indicates if a transfer is busy
end

endmodule

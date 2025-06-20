module axi_stream_pin_source (
    input  wire        aclk,
    input  wire        aresetn,
    
    // Data Input
    input  wire [7:0]  data_pins,
    
    // AXI4-Stream Master Interface (Output)
    output reg         m_axis_tvalid,
    output reg [63:0]  m_axis_tdata,
    output reg         m_axis_tlast,
    output wire [7:0]  m_axis_tkeep,
    output wire [7:0]  m_axis_tstrb,
    output wire [1:0]  m_axis_tdest,
    output wire [3:0]  m_axis_tid,
    input  wire        m_axis_tready
);

    // State machine states
    localparam COLLECT = 1'b0;
    localparam ACTIVE  = 1'b1;
    
    reg state, next_state;
    reg [2:0] byte_counter;        // 0-7 for collecting 8 bytes
    reg [63:0] data_accumulator;   // Accumulate 8 bytes into 64-bit word
    
    // Registered input data for timing closure
    reg [7:0] data_pins_reg;
    
    // Fixed AXI Stream sideband signals
    assign m_axis_tkeep = 8'hFF;    // All bytes valid
    assign m_axis_tstrb = 8'hFF;    // All bytes valid
    assign m_axis_tdest = 2'b00;    // Default destination
    assign m_axis_tid   = 4'b0000;  // Default ID
    
    // State machine - sequential logic
    always @(posedge aclk) begin
        if (!aresetn) begin
            state <= COLLECT;
        end else begin
            state <= next_state;
        end
    end
    
    // Input registration for timing closure
    always @(posedge aclk) begin
        if (!aresetn) begin
            data_pins_reg <= 8'd0;
        end else begin
            data_pins_reg <= data_pins;
        end
    end
    
    // State machine - combinational logic
    always @(*) begin
        next_state = state;
        
        case (state)
            COLLECT: begin
                if (byte_counter == 3'd7) begin
                    next_state = ACTIVE;
                end
            end
            
            ACTIVE: begin
                if (m_axis_tvalid && m_axis_tready) begin
                    next_state = COLLECT;
                end
            end
        endcase
    end
    
    // Data collection logic
    always @(posedge aclk) begin
        if (!aresetn) begin
            byte_counter <= 3'd0;
            data_accumulator <= 64'd0;
        end else begin
            case (state)
                COLLECT: begin
                    // Collect bytes into 64-bit word (little-endian)
                    case (byte_counter)
                        3'd0: data_accumulator[7:0]   <= data_pins_reg;
                        3'd1: data_accumulator[15:8]  <= data_pins_reg;
                        3'd2: data_accumulator[23:16] <= data_pins_reg;
                        3'd3: data_accumulator[31:24] <= data_pins_reg;
                        3'd4: data_accumulator[39:32] <= data_pins_reg;
                        3'd5: data_accumulator[47:40] <= data_pins_reg;
                        3'd6: data_accumulator[55:48] <= data_pins_reg;
                        3'd7: data_accumulator[63:56] <= data_pins_reg;
                    endcase
                    
                    if (byte_counter == 3'd7) begin
                        byte_counter <= 3'd0;
                    end else begin
                        byte_counter <= byte_counter + 1;
                    end
                end
                
                ACTIVE: begin
                    if (m_axis_tvalid && m_axis_tready) begin
                        // Reset accumulator for next word collection
                        data_accumulator <= 64'd0;
                    end
                end
            endcase
        end
    end
    
    // AXI Stream output generation
    always @(posedge aclk) begin
        if (!aresetn) begin
            m_axis_tdata  <= 64'd0;
            m_axis_tvalid <= 1'b0;
            m_axis_tlast  <= 1'b0;
        end else begin
            case (next_state)
                COLLECT: begin
                    m_axis_tvalid <= 1'b0;
                    m_axis_tlast  <= 1'b0;
                    if (state == ACTIVE && m_axis_tvalid && m_axis_tready) begin
                        // Clear data after successful handshake
                        m_axis_tdata <= 64'd0;
                    end
                end
                
                ACTIVE: begin
                    m_axis_tvalid <= 1'b1;
                    m_axis_tdata  <= data_accumulator;
                    m_axis_tlast  <= 1'b0;  // Never assert TLAST for continuous stream
                end
            endcase
        end
    end
    
    // Assertions for debugging (synthesis tool dependent)
    `ifdef SIMULATION
        always @(posedge aclk) begin
            if (aresetn) begin
                // Check that tvalid doesn't drop while tready is low
                if ($past(m_axis_tvalid) && $past(!m_axis_tready) && aresetn) begin
                    assert(m_axis_tvalid) else $error("TVALID dropped while TREADY was low");
                end
                
                // Check that data doesn't change while handshake is pending
                if ($past(m_axis_tvalid) && $past(!m_axis_tready) && m_axis_tvalid && aresetn) begin
                    assert($stable(m_axis_tdata)) else $error("TDATA changed during pending handshake");
                    assert($stable(m_axis_tlast)) else $error("TLAST changed during pending handshake");
                end
                
                // Check byte counter bounds
                assert(byte_counter <= 3'd7) else $error("Byte counter out of bounds");
                
                // TLAST should never be asserted in continuous mode
                assert(!m_axis_tlast) else $error("TLAST asserted in continuous stream mode");
            end
        end
    `endif

endmodule

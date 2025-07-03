/**
 * @brief A minimal, compliant AXI4-Stream master.
 *
 * @details This module implements a basic AXI4-Stream master that sends a
 * packet of a predefined length when triggered. It is designed to be
 * compliant with the AMBA AXI4-Stream Protocol Specification
 * (IHI 0051A) while being as simple as possible.
 *
 * Functionality:
 * 1. Waits in an IDLE state.
 * 2. On a rising edge of the `start` signal, it transitions to the
 * STREAM state.
 * 3. In the STREAM state, it drives a data pattern (an incrementing
 * counter) onto the TDATA bus and asserts TVALID.
 * 4. It holds TDATA and TVALID until the slave asserts TREADY.
 * 5. A transfer occurs when both TVALID and TREADY are high.
 * 6. It sends a configurable number of transfers (TRANSFERS_PER_PACKET).
 * 7. On the final transfer, it asserts TLAST.
 * 8. After the packet is sent, it returns to the IDLE state.
 *
 * This master does not implement optional signals like TKEEP, TSTRB,
 * TID, TDEST, or TUSER, which is compliant with the specification as
 * AXI4-Stream allows for their omission. This makes it compatible
 * with a wide range of slaves, including the CoreAXI4DMAController,
 * which expects fully packed data streams.
 *
 * @param DATA_WIDTH          Width of the TDATA bus in bits.
 * @param TRANSFERS_PER_PACKET Number of data transfers in a single packet.
 */
module axis_master #(
    parameter int DATA_WIDTH = 32,
    parameter int TRANSFERS_PER_PACKET = 16
) (
    // System Signals
    input  logic ACLK,
    input  logic ARESETn,

    // Control/Status Signals
    input  logic start, // Pulse to start sending a packet
    output logic busy,  // High when the master is actively sending a packet

    // AXI4-Stream Master Interface
    output logic [DATA_WIDTH-1:0] M_AXIS_TDATA,
    output logic                   M_AXIS_TVALID,
    input  logic                   M_AXIS_TREADY,
    output logic                   M_AXIS_TLAST
);

    // Internal state representation
    typedef enum logic [1:0] {
        IDLE,
        STREAM,
        LAST
    } state_t;

    state_t current_state, next_state;

    // Internal counters and registers
    logic [DATA_WIDTH-1:0] data_reg;
    logic [$clog2(TRANSFERS_PER_PACKET)-1:0] transfer_counter;

    // AXI handshake signal: a transfer occurs when both valid and ready are high
    wire transfer_fire = M_AXIS_TVALID & M_AXIS_TREADY;

    // --------------------------------------------------------------------------
    // State Machine: Sequential Logic (State Register)
    // --------------------------------------------------------------------------
    always_ff @(posedge ACLK or negedge ARESETn) begin
        if (!ARESETn) begin
            current_state <= IDLE;
        end else begin
            current_state <= next_state;
        end
    end

    // --------------------------------------------------------------------------
    // State Machine: Combinational Logic (Next State Logic and Outputs)
    // --------------------------------------------------------------------------
    always_comb begin
        // Default assignments to avoid latches
        next_state    = current_state;
        M_AXIS_TVALID = 1'b0;
        M_AXIS_TLAST  = 1'b0;
        busy          = (current_state != IDLE);

        case (current_state)
            IDLE: begin
                M_AXIS_TVALID = 1'b0;
                M_AXIS_TLAST  = 1'b0;
                if (start) begin
                    // Start of a new packet
                    if (TRANSFERS_PER_PACKET == 1) begin
                        next_state = LAST; // Handle single-transfer packets
                    end else begin
                        next_state = STREAM;
                    end
                end
            end

            STREAM: begin
                M_AXIS_TVALID = 1'b1;
                M_AXIS_TLAST  = 1'b0;

                // Check if we are about to send the second-to-last transfer
                if (transfer_fire && (transfer_counter == TRANSFERS_PER_PACKET - 2)) begin
                    next_state = LAST;
                end
            end

            LAST: begin
                M_AXIS_TVALID = 1'b1;
                M_AXIS_TLAST  = 1'b1; // Assert TLAST on the final transfer

                if (transfer_fire) begin
                    next_state = IDLE; // Packet finished, return to idle
                end
            end

            default: begin
                next_state = IDLE;
            end
        endcase
    end

    // --------------------------------------------------------------------------
    // Datapath Logic: Counters and Registers
    // --------------------------------------------------------------------------
    always_ff @(posedge ACLK or negedge ARESETn) begin
        if (!ARESETn) begin
            data_reg         <= {DATA_WIDTH{1'b0}};
            transfer_counter <= '0;
            M_AXIS_TDATA     <= {DATA_WIDTH{1'b0}};
        end else begin
            if (current_state == IDLE && start) begin
                // Initialize counters at the beginning of a packet
                transfer_counter <= '0;
                data_reg         <= '0;
            end else if (transfer_fire) begin
                // When a transfer is successful, increment counters
                transfer_counter <= transfer_counter + 1;
                data_reg         <= data_reg + 1;
            end
            
            // Drive the data output from the register
            // This ensures TDATA is stable when TVALID is high, as per spec.
            M_AXIS_TDATA <= data_reg;
        end
    end

endmodule

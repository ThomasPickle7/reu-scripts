# Exporting core AXI4StreamMaster to TCL
# Exporting Create HDL core command for module AXI4StreamMaster
create_hdl_core -file {hdl/axi_stream_source.sv} -module {AXI4StreamMaster} -library {work} -package {}
# Exporting BIF information of  HDL core command for module AXI4StreamMaster
hdl_core_add_bif -hdl_core_name {AXI4StreamMaster} -bif_definition {AXI4:AMBA:AMBA4:slave} -bif_name {BIF_1} -signal_map {\
"AWADDR:S_AXI_AWADDR" \
"AWPROT:S_AXI_AWPROT" \
"AWVALID:S_AXI_AWVALID" \
"AWREADY:S_AXI_AWREADY" \
"WDATA:S_AXI_WDATA" \
"WSTRB:S_AXI_WSTRB" \
"WVALID:S_AXI_WVALID" \
"WREADY:S_AXI_WREADY" \
"BRESP:S_AXI_BRESP" \
"BVALID:S_AXI_BVALID" \
"BREADY:S_AXI_BREADY" \
"ARADDR:S_AXI_ARADDR" \
"ARPROT:S_AXI_ARPROT" \
"ARVALID:S_AXI_ARVALID" \
"ARREADY:S_AXI_ARREADY" \
"RDATA:S_AXI_RDATA" \
"RRESP:S_AXI_RRESP" \
"RVALID:S_AXI_RVALID" \
"RREADY:S_AXI_RREADY" }

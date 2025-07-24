puts "==================== Add MEM_INTERFACE option: EXPORT ===================="

download_core -vlnv {Actel:SystemBuilder:PF_SRAM_AHBL_AXI:1.2.111} -location {www.microchip-ip.com/repositories/SgCore}
build_design_hierarchy

source script_support/components/MEM_INTERFACE/EXPORT/hdl_source.tcl
build_design_hierarchy

#Sourcing the Tcl files for creating individual components under the top level
source script_support/components/MEM_INTERFACE/EXPORT/components/DMA_CONTROLLER.tcl 
source script_support/components/MEM_INTERFACE/EXPORT/components/DMA_INITIATOR.tcl 
source script_support/components/MEM_INTERFACE/EXPORT/components/FIC_0_INITIATOR.tcl 
source script_support/components/MEM_INTERFACE/EXPORT/components/AXI4StreamMaster.tcl
source script_support/components/MEM_INTERFACE/EXPORT/components/MEM_INTERFACE.tcl 

build_design_hierarchy

#---------------------------------------------------------------------------------
# Modifications to the BVF_RISCV_SUBSYSTEM to add the MEM_INTERFACE
#---------------------------------------------------------------------------------

source script_support/components/MEM_INTERFACE/EXPORT/CHANGE_BVF_RISCV_SUBSYSTEM.tcl
sd_update_instance -sd_name ${top_level_name} -instance_name {BVF_RISCV_SUBSYSTEM}

#---------------------------------------------------------------------------------
# Create the MEM_INTERFACE block.
# This block will be stiched up to the rest of the design in the calling script.
#---------------------------------------------------------------------------------

set sd_name ${top_level_name}

sd_instantiate_component -sd_name ${sd_name} -component_name {MEM_INTERFACE} -instance_name {FPGA_MEM_INTERFACE_0}

#---------------------------------------------------------------------------------

puts "==================== Connect MEM_INTERFACE_0 to the rest of the design ===================="
#sd_disconnect_pins -sd_name ${sd_name} -pin_names {"BVF_RISCV_SUBSYSTEM:FIC_0_ACLK" "CLOCKS_AND_RESETS:FIC_0_ACLK" "BVF_RISCV_SUBSYSTEM:FIC_3_PCLK" "CLOCKS_AND_RESETS:FIC_3_PCLK" "BVF_RISCV_SUBSYSTEM:PRESETN" "CLOCKS_AND_RESETS:FIC_3_FABRIC_RESET_N" "CLOCKS_AND_RESETS:FIC_0_FABRIC_RESET_N" "PHY_RSTn" "USB0_RESETB"}
sd_connect_pins -sd_name ${sd_name} -pin_names {"CLOCKS_AND_RESETS:FIC_0_ACLK" "FPGA_MEM_INTERFACE_0:ACLK" }
sd_connect_pins -sd_name ${sd_name} -pin_names {"BVF_RISCV_SUBSYSTEM:MSS_INT_F2M_3" "FPGA_MEM_INTERFACE_0:DMA_CONTROLLER_IRQ" }
sd_connect_pins -sd_name ${sd_name} -pin_names {"CLOCKS_AND_RESETS:FIC_0_FABRIC_RESET_N" "FPGA_MEM_INTERFACE_0:ARESETN"}

#---------------------------------------------------------------------------------

sd_connect_pins -sd_name ${sd_name} -pin_names {"BVF_RISCV_SUBSYSTEM:FIC_0_AXI4_INITIATOR" "FPGA_MEM_INTERFACE_0:AXI4mmaster0" }
sd_connect_pins -sd_name ${sd_name} -pin_names {"BVF_RISCV_SUBSYSTEM:FIC_0_AXI4_TARGET" "FPGA_MEM_INTERFACE_0:AXI4mslave0" }

#---------------------------------------------------------------------------------
# Settings to make the design compile
#---------------------------------------------------------------------------------

sd_connect_pins_to_constant -sd_name ${sd_name} -pin_names {BVF_RISCV_SUBSYSTEM:MSS_INT_F2M_4_7} -value {GND} 

smartdesign -memory_map_drc_change_error_to_warning true \
           -bus_interface_data_width_drc_change_error_to_warning true \
           -bus_interface_id_width_drc_change_error_to_warning true

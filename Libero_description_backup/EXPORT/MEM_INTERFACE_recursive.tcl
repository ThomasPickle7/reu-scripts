#This Tcl file sources other Tcl files to build the design(on which recursive export is run) in a bottom-up fashion

#Sourcing the Tcl file in which all the HDL source files used in the design are imported or linked
source hdl_source.tcl
build_design_hierarchy

#Sourcing the Tcl files in which HDL+ core definitions are created for HDL modules
source components/AXI4StreamMaster.tcl 
build_design_hierarchy

#Sourcing the Tcl files for creating individual components under the top level
source components/DMA_CONTROLLER.tcl 
source components/DMA_INITIATOR.tcl 
source components/FIC_0_INITIATOR.tcl 
source components/MEM_INTERFACE.tcl 
build_design_hierarchy

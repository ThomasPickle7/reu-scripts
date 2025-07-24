set sd_name {BVF_RISCV_SUBSYSTEM}

open_smartdesign -sd_name ${sd_name}

#-------------------------------------------------------------------------------
# Slicing the BVF_RISCV_SUBSYSTEM MSS Interrupts
#-------------------------------------------------------------------------------

sd_delete_ports -sd_name ${sd_name} -port_names {MSS_INT_F2M_3_7} 
sd_delete_pin_slices -sd_name ${sd_name} -pin_name {PF_SOC_MSS:MSS_INT_F2M} -pin_slices {"[7:3]"}
sd_create_pin_slices -sd_name ${sd_name} -pin_name {PF_SOC_MSS:MSS_INT_F2M} -pin_slices {"[7:4]"}
sd_create_pin_slices -sd_name ${sd_name} -pin_name {PF_SOC_MSS:MSS_INT_F2M} -pin_slices {"[3]"}
sd_create_bus_port -sd_name ${sd_name} -port_name {MSS_INT_F2M_4_7} -port_direction {IN} -port_range {[7:4]}
sd_create_scalar_port -sd_name ${sd_name} -port_name {MSS_INT_F2M_3} -port_direction {IN}
sd_connect_pins -sd_name ${sd_name} -pin_names {"MSS_INT_F2M_4_7" "PF_SOC_MSS:MSS_INT_F2M[7:4]"} 
sd_connect_pins -sd_name ${sd_name} -pin_names {"MSS_INT_F2M_3" "PF_SOC_MSS:MSS_INT_F2M[3]"}

#-------------------------------------------------------------------------------
# Save the SmartDesign
#-------------------------------------------------------------------------------
save_smartdesign -sd_name ${sd_name}

#-------------------------------------------------------------------------------
# Generate SmartDesign BVF_RISCV_SUBSYSTEM
#-------------------------------------------------------------------------------
generate_component -component_name ${sd_name}

####
# F Prime CMakeLists.txt:
#
# SOURCES: list of source files (to be compiled)
# AUTOCODER_INPUTS: list of files to be passed to the autocoders
# DEPENDS: list of libraries that this module depends on
#
# Per-board radio selection (Phase 4 — USP port):
#   - v5e (CONFIG_LORA_BASICS_MODEM_DRIVERS=y): RadioInstances_Usp.fpp +
#       RadioTopology_Usp.fpp  →  Zephyr::UspRadio active component
#   - v5c / v5d (CONFIG_LORA_BASICS_MODEM_DRIVERS not set): RadioInstances_Lora.fpp +
#       RadioTopology_Lora.fpp →  Zephyr::LoRa passive driver  (legacy path)
#
# More information in the F´ CMake API documentation:
# https://fprime.jpl.nasa.gov/latest/docs/reference/api/cmake/API/
####

if(DEFINED CONFIG_LORA_BASICS_MODEM_DRIVERS)
    set(RADIO_INSTANCES_FPP "${CMAKE_CURRENT_LIST_DIR}/RadioInstances_Usp.fpp")
    set(RADIO_TOPOLOGY_FPP  "${CMAKE_CURRENT_LIST_DIR}/RadioTopology_Usp.fpp")
else()
    set(RADIO_INSTANCES_FPP "${CMAKE_CURRENT_LIST_DIR}/RadioInstances_Lora.fpp")
    set(RADIO_TOPOLOGY_FPP  "${CMAKE_CURRENT_LIST_DIR}/RadioTopology_Lora.fpp")
endif()

register_fprime_module(
    AUTOCODER_INPUTS
        "${CMAKE_CURRENT_LIST_DIR}/instances.fpp"
        "${RADIO_INSTANCES_FPP}"
        "${CMAKE_CURRENT_LIST_DIR}/topology.fpp"
        "${RADIO_TOPOLOGY_FPP}"
        "${CMAKE_CURRENT_LIST_DIR}/ReferenceDeploymentPackets.fppi"
    SOURCES
        "${CMAKE_CURRENT_LIST_DIR}/ReferenceDeploymentTopology.cpp"
    DEPENDS
        Fw_Logger
)

####
# F Prime CMakeLists.txt:
#
# SOURCES: list of source files (to be compiled)
# AUTOCODER_INPUTS: list of files to be passed to the autocoders
# DEPENDS: list of libraries that this module depends on
#
# Per-board radio selection:
#   CONFIG_LORA_BASICS_MODEM_DRIVERS set   (v5e):     Radio*.fppi -> Radio*_Usp.fppi  (Zephyr::UspRadio)
#   CONFIG_LORA_BASICS_MODEM_DRIVERS unset (v5c/v5d): Radio*.fppi -> Radio*_Lora.fppi (Zephyr::LoRa)
# FPP `include` resolves relative to the including file, so the links are
# created in the source tree at configure time. COPY_ON_ERROR falls back to a
# file copy where symlinks are not supported. The links are gitignored.
####

if(DEFINED CONFIG_LORA_BASICS_MODEM_DRIVERS)
    set(RADIO_SUFFIX "Usp")
else()
    set(RADIO_SUFFIX "Lora")
endif()

# Instance and topology snippets.
foreach(_kind "Instances" "Topology")
    set(_src "${CMAKE_CURRENT_LIST_DIR}/Radio${_kind}_${RADIO_SUFFIX}.fppi")
    set(_dst "${CMAKE_CURRENT_LIST_DIR}/Radio${_kind}.fppi")
    file(CREATE_LINK "${_src}" "${_dst}" SYMBOLIC COPY_ON_ERROR)
endforeach()
# Packet snippets (they reference the per-board instance names).
foreach(_kind "BytesReceived" "Radio")
    set(_src "${CMAKE_CURRENT_LIST_DIR}/RadioPackets${_kind}_${RADIO_SUFFIX}.fppi")
    set(_dst "${CMAKE_CURRENT_LIST_DIR}/RadioPackets${_kind}.fppi")
    file(CREATE_LINK "${_src}" "${_dst}" SYMBOLIC COPY_ON_ERROR)
endforeach()

register_fprime_module(
    AUTOCODER_INPUTS
        "${CMAKE_CURRENT_LIST_DIR}/instances.fpp"
        "${CMAKE_CURRENT_LIST_DIR}/topology.fpp"
        "${CMAKE_CURRENT_LIST_DIR}/ReferenceDeploymentPackets.fppi"
    SOURCES
        "${CMAKE_CURRENT_LIST_DIR}/ReferenceDeploymentTopology.cpp"
    DEPENDS
        Fw_Logger
        Svc_TlmPacketizer_config_TlmPacketizerConfig
)

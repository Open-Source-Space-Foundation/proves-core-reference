####
# F Prime CMakeLists.txt:
#
# SOURCES: list of source files (to be compiled)
# AUTOCODER_INPUTS: list of files to be passed to the autocoders
# DEPENDS: list of libraries that this module depends on
#
# Per-board radio selection (Phase 4 — USP port):
#   v5e (CONFIG_LORA_BASICS_MODEM_DRIVERS=y):
#       RadioInstances.fppi -> RadioInstances_Usp.fppi   (Zephyr::UspRadio)
#       RadioTopology.fppi  -> RadioTopology_Usp.fppi
#   v5c / v5d (CONFIG_LORA_BASICS_MODEM_DRIVERS not set):
#       RadioInstances.fppi -> RadioInstances_Lora.fppi  (Zephyr::LoRa)
#       RadioTopology.fppi  -> RadioTopology_Lora.fppi
#
# FPP `include` directives resolve relative to the source file, so we
# create symbolic links in the source tree at configure time.  The links
# are committed to .gitignore so they do not appear as untracked files.
#
# More information in the F´ CMake API documentation:
# https://fprime.jpl.nasa.gov/latest/docs/reference/api/cmake/API/
####

if(DEFINED CONFIG_LORA_BASICS_MODEM_DRIVERS)
    set(RADIO_SUFFIX "Usp")
else()
    set(RADIO_SUFFIX "Lora")
endif()

# Generate per-board .fppi symlinks in the source dir.
# FPP `include` resolves relative to the source file, so symlinks live
# alongside the FPP files.  CREATE_LINK SYMBOLIC COPY_ON_ERROR falls back
# to a file copy on platforms that don't support symlinks.
foreach(_kind "Instances" "Topology")
    set(_src "${CMAKE_CURRENT_LIST_DIR}/Radio${_kind}_${RADIO_SUFFIX}.fppi")
    set(_dst "${CMAKE_CURRENT_LIST_DIR}/Radio${_kind}.fppi")
    file(CREATE_LINK "${_src}" "${_dst}" SYMBOLIC COPY_ON_ERROR)
endforeach()
# Packet snippet symlinks (telemetry packet definitions reference per-board instance names)
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

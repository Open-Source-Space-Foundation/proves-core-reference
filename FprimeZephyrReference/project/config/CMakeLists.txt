####
# default/config/CMakeLists.txt:
#
# Sets a list of source files for cmake to process as part of autocoding.
####

# Configure spacecraft ID (can be overridden via CMake option)
set(FPRIME_SPACECRAFT_ID "0x0044" CACHE STRING "Spacecraft ID (10 bits, range 0x0000-0x03FF) - default 0x0044 for development, use 0x00C1 for CI")

# Generate ComCfg.fpp from template with configured spacecraft ID
configure_file(
    "${CMAKE_CURRENT_LIST_DIR}/ComCfg.fpp.in"
    "${CMAKE_CURRENT_LIST_DIR}/ComCfg.fpp"
    @ONLY
)

register_fprime_config(
    CONFIGURATION_OVERRIDES
        "${CMAKE_CURRENT_LIST_DIR}/CdhCoreConfig.fpp"
        "${CMAKE_CURRENT_LIST_DIR}/CdhCoreTlmConfig.fpp"
        "${CMAKE_CURRENT_LIST_DIR}/CdhCoreFatalHandlerConfig.fpp"
        "${CMAKE_CURRENT_LIST_DIR}/ComCcsdsConfig.fpp"
        "${CMAKE_CURRENT_LIST_DIR}/ComCfg.fpp"
        "${CMAKE_CURRENT_LIST_DIR}/CommandDispatcherImplCfg.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/LoRaCfg.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/FpConfig.h"
        "${CMAKE_CURRENT_LIST_DIR}/TlmPacketizerCfg.hpp"
    INTERFACE
)

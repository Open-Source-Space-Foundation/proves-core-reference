####
# default/config/CMakeLists.txt:
#
# Sets a list of source files for cmake to process as part of autocoding.
####

# Select ComCfg file based on build environment
# Use ComCfg.CI.fpp for CI builds, ComCfg.fpp for development
if(DEFINED ENV{CI})
    set(COM_CFG_FILE "${CMAKE_CURRENT_LIST_DIR}/ComCfg.CI.fpp")
    message(STATUS "Using CI spacecraft ID configuration")
else()
    set(COM_CFG_FILE "${CMAKE_CURRENT_LIST_DIR}/ComCfg.fpp")
    message(STATUS "Using development spacecraft ID configuration")
endif()

register_fprime_config(
    CONFIGURATION_OVERRIDES
        "${CMAKE_CURRENT_LIST_DIR}/CdhCoreConfig.fpp"
        "${CMAKE_CURRENT_LIST_DIR}/CdhCoreTlmConfig.fpp"
        "${CMAKE_CURRENT_LIST_DIR}/CdhCoreFatalHandlerConfig.fpp"
        "${CMAKE_CURRENT_LIST_DIR}/ComCcsdsConfig.fpp"
        "${COM_CFG_FILE}"
        "${CMAKE_CURRENT_LIST_DIR}/CommandDispatcherImplCfg.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/LoRaCfg.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/FpConfig.h"
        "${CMAKE_CURRENT_LIST_DIR}/TlmPacketizerCfg.hpp"
    INTERFACE
)

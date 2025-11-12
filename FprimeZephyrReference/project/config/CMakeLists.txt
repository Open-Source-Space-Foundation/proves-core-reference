####
# default/config/CMakeLists.txt:
#
# Sets a list of source files for cmake to process as part of autocoding.
####

# Select ComCfg file based on build environment
# Use ComCfg.CI.fpp for CI builds, ComCfg.fpp for development
if(DEFINED ENV{CI})
    message(STATUS "Using CI spacecraft ID configuration")
    set(COMCFG_FILE "${CMAKE_CURRENT_LIST_DIR}/ComCfg.CI.fpp")
else()
    message(STATUS "Using development spacecraft ID configuration")
    set(COMCFG_FILE "${CMAKE_CURRENT_LIST_DIR}/ComCfg.fpp")
endif()

register_fprime_config(
    SOURCES
        "${COMCFG_FILE}"
    CONFIGURATION_OVERRIDES
        "${CMAKE_CURRENT_LIST_DIR}/AcConstants.fpp"
        "${CMAKE_CURRENT_LIST_DIR}/CdhCoreConfig.fpp"
        "${CMAKE_CURRENT_LIST_DIR}/CdhCoreTlmConfig.fpp"
        "${CMAKE_CURRENT_LIST_DIR}/CdhCoreFatalHandlerConfig.fpp"
        "${CMAKE_CURRENT_LIST_DIR}/ComCcsdsConfig.fpp"
        "${CMAKE_CURRENT_LIST_DIR}/CommandDispatcherImplCfg.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/FileHandlingConfig.fpp"
        "${CMAKE_CURRENT_LIST_DIR}/FpConfig.h"
        "${CMAKE_CURRENT_LIST_DIR}/FpConstants.fpp"
        "${CMAKE_CURRENT_LIST_DIR}/LoRaCfg.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/PlatformCfg.fpp"
        "${CMAKE_CURRENT_LIST_DIR}/TlmPacketizerCfg.hpp"
    INTERFACE
)

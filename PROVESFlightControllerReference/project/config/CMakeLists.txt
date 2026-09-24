####
# default/config/CMakeLists.txt:
#
# Sets a list of source files for cmake to process as part of autocoding.
####
set(PROJECT_CONFIG_OVERRIDES
    "${CMAKE_CURRENT_LIST_DIR}/AcConstants.fpp"
    "${CMAKE_CURRENT_LIST_DIR}/CdhCoreConfig.fpp"
    "${CMAKE_CURRENT_LIST_DIR}/CdhCoreTlmConfig.fpp"
    "${CMAKE_CURRENT_LIST_DIR}/CdhCoreFatalHandlerConfig.fpp"
    "${CMAKE_CURRENT_LIST_DIR}/ComCcsdsConfig.fpp"
    "${CMAKE_CURRENT_LIST_DIR}/ComCfg.fpp"
    "${CMAKE_CURRENT_LIST_DIR}/CommandDispatcherImplCfg.hpp"
    "${CMAKE_CURRENT_LIST_DIR}/FileHandlingConfig.fpp"
    "${CMAKE_CURRENT_LIST_DIR}/FpConfig.h"
    "${CMAKE_CURRENT_LIST_DIR}/FpConstants.fpp"
    "${CMAKE_CURRENT_LIST_DIR}/PlatformCfg.fpp"
    "${CMAKE_CURRENT_LIST_DIR}/TlmPacketizerCfg.fpp"
    "${CMAKE_CURRENT_LIST_DIR}/TlmPacketizerCfg.hpp"
    "${CMAKE_CURRENT_LIST_DIR}/UpdateConfig.fpp"
    "${CMAKE_CURRENT_LIST_DIR}/UpdateStatus.fpp"
)
# LoRaCfg.hpp overrides a header that only exists in fprime-zephyr's config;
# native builds exclude fprime-zephyr (see native/settings.ini).
if(FPRIME_PLATFORM STREQUAL "Zephyr")
    list(APPEND PROJECT_CONFIG_OVERRIDES "${CMAKE_CURRENT_LIST_DIR}/LoRaCfg.hpp")
endif()
register_fprime_config(
    CONFIGURATION_OVERRIDES
        ${PROJECT_CONFIG_OVERRIDES}
    INTERFACE
)

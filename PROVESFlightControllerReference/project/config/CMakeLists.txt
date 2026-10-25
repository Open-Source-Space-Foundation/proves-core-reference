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
# 64-bit hosts (native_sim/native/64) need bigger OS handles than the 32-bit flight
# boards, so build from a copy of PlatformCfg.fpp with every handle size doubled.
if(CONFIG_64BIT)
    file(READ "${CMAKE_CURRENT_LIST_DIR}/PlatformCfg.fpp" PLATFORM_CFG)
    string(REGEX MATCHALL "FW_[A-Z_]+_HANDLE_MAX_SIZE = [0-9]+" HANDLE_SIZES "${PLATFORM_CFG}")
    foreach(HANDLE_SIZE IN LISTS HANDLE_SIZES)
        string(REGEX REPLACE " = .*" "" NAME "${HANDLE_SIZE}")
        string(REGEX REPLACE ".* = " "" SIZE "${HANDLE_SIZE}")
        math(EXPR SIZE "${SIZE} * 2")
        string(REPLACE "${HANDLE_SIZE}" "${NAME} = ${SIZE}" PLATFORM_CFG "${PLATFORM_CFG}")
    endforeach()
    file(CONFIGURE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/PlatformCfg.fpp" CONTENT "${PLATFORM_CFG}" @ONLY)
    list(TRANSFORM PROJECT_CONFIG_OVERRIDES REPLACE "^${CMAKE_CURRENT_LIST_DIR}/PlatformCfg.fpp$" "${CMAKE_CURRENT_BINARY_DIR}/PlatformCfg.fpp")
endif()
register_fprime_config(
    CONFIGURATION_OVERRIDES
        ${PROJECT_CONFIG_OVERRIDES}
    INTERFACE
)

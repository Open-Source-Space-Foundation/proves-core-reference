####
# default/config/CMakeLists.txt:
#
# Sets a list of source files for cmake to process as part of autocoding.
####
# The flight controller's update worker is FlashWorker. The Nicla image does not
# include that worker, so it keeps the framework stub status.
if(BOARD MATCHES "proves_flight_control_board")
    set(PROVES_UPDATE_CONFIG
        "${CMAKE_CURRENT_LIST_DIR}/UpdateConfig.fpp"
        "${CMAKE_CURRENT_LIST_DIR}/UpdateStatus.fpp"
    )
endif()

register_fprime_config(
    CONFIGURATION_OVERRIDES
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
        "${CMAKE_CURRENT_LIST_DIR}/LoRaCfg.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/PlatformCfg.fpp"
        "${CMAKE_CURRENT_LIST_DIR}/TlmPacketizerCfg.fpp"
        "${CMAKE_CURRENT_LIST_DIR}/TlmPacketizerCfg.hpp"
        ${PROVES_UPDATE_CONFIG}
    INTERFACE
)

####
# This sets up the build system for the 'fprime-zephyr-reference' project, including
# components and deployments from project.cmake. In addition, it imports the core F Prime components.
####

cmake_minimum_required(VERSION 3.24.2)
add_compile_definitions(_POSIX_C_SOURCE=200809L)

# Set BOARD_ROOT to find custom boards
# The structure is BOARD_ROOT/boards/vendor/board
list(APPEND BOARD_ROOT "${CMAKE_CURRENT_LIST_DIR}")

# Patch in std-atomic implementations
if (BOARD STREQUAL "rpi_pico" OR FPRIME_ZEPHYR_USE_STD_ATOMIC_FIX)
    include_directories(BEFORE "${CMAKE_CURRENT_LIST_DIR}/lib/fprime-zephyr/fprime-zephyr/Os/StdAtomic")
endif()
if (NOT BUILD_TESTING)
    find_package(Zephyr HINTS "${CMAKE_CURRENT_LIST_DIR}/lib/zephyr-workspace")
endif()
project(fprime-zephyr-reference C CXX)

###
# RadioLib Integration
# Add RadioLib Zephyr HAL adapter
###
add_subdirectory("${CMAKE_CURRENT_LIST_DIR}/lib/radiolib-zephyr-hal" radiolib_zephyr_hal)

# Add RadioLib sources to app target
# Only include SX128x module sources (we only need SX1280)
# Also include core RadioLib files needed by all modules
set(RADIOLIB_SOURCES
    # Core RadioLib files
    "${CMAKE_CURRENT_LIST_DIR}/lib/radiolib/src/Module.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/lib/radiolib/src/Hal.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/lib/radiolib/src/utils/Utils.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/lib/radiolib/src/utils/CRC.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/lib/radiolib/src/utils/Cryptography.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/lib/radiolib/src/utils/FEC.cpp"
    # PhysicalLayer base class (required by all radio modules)
    "${CMAKE_CURRENT_LIST_DIR}/lib/radiolib/src/protocols/PhysicalLayer/PhysicalLayer.cpp"
    # SX128x module files (for SX1280 support)
    # Note: SX1280 inherits from SX1281, so we need both
    "${CMAKE_CURRENT_LIST_DIR}/lib/radiolib/src/modules/SX128x/SX128x.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/lib/radiolib/src/modules/SX128x/SX1281.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/lib/radiolib/src/modules/SX128x/SX1280.cpp"
)

# Add RadioLib sources to app target
target_sources(app PRIVATE ${RADIOLIB_SOURCES})
target_include_directories(app PRIVATE
    "${CMAKE_CURRENT_LIST_DIR}/lib/radiolib/src"
    "${CMAKE_CURRENT_LIST_DIR}/lib/radiolib-zephyr-hal"
)
# Link the Zephyr HAL library (zephyr_library creates a library that needs to be linked)
target_link_libraries(app PRIVATE ..__..__radiolib-zephyr-hal)

# RadioLib build options - use generic build (don't define RADIOLIB_BUILD_ARDUINO at all)
# RadioLib will automatically detect generic build if RADIOLIB_BUILD_ARDUINO is not defined
# Exclude unnecessary modules to reduce compilation time and dependencies
# Exclude modules we don't need to reduce compilation time and dependencies
add_compile_definitions(RADIOLIB_EXCLUDE_CC1101=1)
add_compile_definitions(RADIOLIB_EXCLUDE_LLCC68=1)
add_compile_definitions(RADIOLIB_EXCLUDE_LR11X0=1)
add_compile_definitions(RADIOLIB_EXCLUDE_NRF24=1)
add_compile_definitions(RADIOLIB_EXCLUDE_RF69=1)
add_compile_definitions(RADIOLIB_EXCLUDE_RFM2X=1)
add_compile_definitions(RADIOLIB_EXCLUDE_SI443X=1)
add_compile_definitions(RADIOLIB_EXCLUDE_SX123X=1)
add_compile_definitions(RADIOLIB_EXCLUDE_SX126X=1)
add_compile_definitions(RADIOLIB_EXCLUDE_SX127X=1)

# Put in an option to force the device back into programmable state for automated testing
option(FPRIME_CI_FAILSAFE_ENABLED "Cycle count before a forced reset to programmable state" OFF)
if (FPRIME_CI_FAILSAFE_ENABLED)
    set(FPRIME_CI_FAILSAFE_CYCLE_COUNT 6000 CACHE STRING "Cycles before a forced reset")
    add_compile_definitions(FPRIME_CI_FAILSAFE_CYCLE_COUNT=${FPRIME_CI_FAILSAFE_CYCLE_COUNT})
endif()

###
# F' Core Setup
# This includes all of the F prime core components, and imports the make-system.
###
include("${CMAKE_CURRENT_LIST_DIR}/lib/fprime/cmake/FPrime.cmake")
# NOTE: register custom targets between these two lines
fprime_setup_included_code()

# This includes project-wide objects
add_fprime_subdirectory("${CMAKE_CURRENT_LIST_DIR}/FprimeZephyrReference")
if (FPRIME_USE_POSIX)
    set_target_properties(Drv_Ip PROPERTIES EXCLUDE_FROM_ALL TRUE)
    set_target_properties(Drv_TcpServer PROPERTIES EXCLUDE_FROM_ALL TRUE)
    set_target_properties(Drv_TcpClient PROPERTIES EXCLUDE_FROM_ALL TRUE)
    set_target_properties(Drv_Udp PROPERTIES EXCLUDE_FROM_ALL TRUE)
endif()
set_target_properties(Svc_FatalHandler PROPERTIES EXCLUDE_FROM_ALL TRUE)
set_target_properties(fprime-zephyr_Drv_ZephyrSpiDriver PROPERTIES EXCLUDE_FROM_ALL TRUE)

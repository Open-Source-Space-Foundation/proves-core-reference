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

# When running tests, avoid pulling in Zephyr (no BOARD etc. needed)
if (NOT BUILD_TESTING)
    find_package(Zephyr HINTS "${CMAKE_CURRENT_LIST_DIR}/lib/zephyr-workspace")
endif()

project(fprime-zephyr-reference C CXX)

# Enable Zephyr Pico hardware_adc library
zephyr_include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/lib/zephyr-workspace/modules/hal/rpi_pico/src/rp2_common/hardware_adc/include
)

# Disable F´ framework/autocoder/app unit tests; we use our own GoogleTests in tests/
set(FPRIME_ENABLE_FRAMEWORK_UTS OFF CACHE BOOL "" FORCE)
set(FPRIME_ENABLE_AUTOCODER_UTS OFF CACHE BOOL "" FORCE)
set(FPRIME_ENABLE_APP_UTS OFF CACHE BOOL "" FORCE)

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
add_fprime_subdirectory("${CMAKE_CURRENT_LIST_DIR}/PROVESFlightControllerReference")

if (FPRIME_USE_POSIX)
    set_target_properties(Drv_Ip PROPERTIES EXCLUDE_FROM_ALL TRUE)
    set_target_properties(Drv_TcpServer PROPERTIES EXCLUDE_FROM_ALL TRUE)
    set_target_properties(Drv_TcpClient PROPERTIES EXCLUDE_FROM_ALL TRUE)
    set_target_properties(Drv_Udp PROPERTIES EXCLUDE_FROM_ALL TRUE)
endif()
set_target_properties(Svc_FatalHandler PROPERTIES EXCLUDE_FROM_ALL TRUE)
set_target_properties(fprime-zephyr_Drv_ZephyrSpiDriver PROPERTIES EXCLUDE_FROM_ALL TRUE)

# If BUILD_TESTING is ON, add our GoogleTest tests
if (BUILD_TESTING)
    enable_testing()
    add_subdirectory(tests)
endif()

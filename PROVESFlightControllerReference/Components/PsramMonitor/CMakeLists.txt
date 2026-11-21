####
# F Prime CMakeLists.txt — PsramMonitor (issue #285, S6)
#
# SOURCES: list of source files (to be compiled)
# AUTOCODER_INPUTS: list of files to be passed to the autocoders
# DEPENDS: list of libraries that this module depends on
#
# More information in the F´ CMake API documentation:
# https://fprime.jpl.nasa.gov/latest/docs/reference/api/cmake/API/
####

register_fprime_library(
    AUTOCODER_INPUTS
        "${CMAKE_CURRENT_LIST_DIR}/PsramMonitor.fpp"
    SOURCES
        "${CMAKE_CURRENT_LIST_DIR}/PsramMonitor.cpp"
)

# When the PSRAM driver is enabled (v5d), PsramMonitor.cpp includes
# "drivers/memc/memc_rp2350_psram.h" which lives at the repo root.
# The pattern mirrors the Authenticate component's use of CMAKE_SOURCE_DIR.
set(PSRAM_MONITOR_TARGET "PROVESFlightControllerReference_Components_PsramMonitor")
if(TARGET ${PSRAM_MONITOR_TARGET})
    target_include_directories(${PSRAM_MONITOR_TARGET} PRIVATE
        "${CMAKE_SOURCE_DIR}"
    )
endif()

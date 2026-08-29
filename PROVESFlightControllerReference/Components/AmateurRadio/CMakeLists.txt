####
# F Prime CMakeLists.txt:
#
# SOURCES: list of source files (to be compiled)
# AUTOCODER_INPUTS: list of files to be passed to the autocoders
# DEPENDS: list of libraries that this module depends on
#
# More information in the F´ CMake API documentation:
# https://fprime.jpl.nasa.gov/latest/docs/reference/api/cmake/API/
#
####

# Module names are derived from the path from the nearest project/library/framework
# root when not specifically overridden by the developer. i.e. The module defined by
# `Ref/SignalGen/CMakeLists.txt` will be named `Ref_SignalGen`.

# Implementation calls Zephyr APIs. On non-Zephyr platforms register autocode
# only, so this module's ports/types stay resolvable to portable consumers
# (see docs/adr/0002).
if(FPRIME_PLATFORM STREQUAL "Zephyr")
    register_fprime_library(
        AUTOCODER_INPUTS
            "${CMAKE_CURRENT_LIST_DIR}/AmateurRadio.fpp"
        SOURCES
            "${CMAKE_CURRENT_LIST_DIR}/AmateurRadio.cpp"
    )
else()
    register_fprime_library(
        AUTOCODER_INPUTS
            "${CMAKE_CURRENT_LIST_DIR}/AmateurRadio.fpp"
    )
endif()

### Unit Tests ###
# register_fprime_ut(
#     AUTOCODER_INPUTS
#         "${CMAKE_CURRENT_LIST_DIR}/AmateurRadio.fpp"
#     SOURCES
#         "${CMAKE_CURRENT_LIST_DIR}/test/ut/AmateurRadioTestMain.cpp"
#         "${CMAKE_CURRENT_LIST_DIR}/test/ut/AmateurRadioTester.cpp"
#     DEPENDS
#         STest # For rules-based testing
#     UT_AUTO_HELPERS
# )

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

register_fprime_library(
    AUTOCODER_INPUTS
        "${CMAKE_CURRENT_LIST_DIR}/SBand.fpp"
    SOURCES
        "${CMAKE_CURRENT_LIST_DIR}/SBand.cpp"
)

# Add include path for out-of-tree Zephyr driver
target_include_directories(FprimeZephyrReference_Components_SBand PUBLIC
    "${CMAKE_CURRENT_LIST_DIR}/../../../include"
)

# RadioLib no longer needed - using Zephyr driver
# add_subdirectory("${FPRIME_PROJECT_ROOT}/lib/RadioLib" "${CMAKE_BINARY_DIR}/RadioLib")
# target_compile_options(RadioLib PRIVATE -w)

### Unit Tests ###
# register_fprime_ut(
#     AUTOCODER_INPUTS
#         "${CMAKE_CURRENT_LIST_DIR}/SBand.fpp"
#     SOURCES
#         "${CMAKE_CURRENT_LIST_DIR}/test/ut/SBandTestMain.cpp"
#         "${CMAKE_CURRENT_LIST_DIR}/test/ut/SBandTester.cpp"
#     DEPENDS
#         STest # For rules-based testing
#     UT_AUTO_HELPERS
# )

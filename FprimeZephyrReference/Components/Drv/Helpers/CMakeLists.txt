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
    SOURCES
        "${CMAKE_CURRENT_LIST_DIR}/Helpers.cpp"
    DEPENDS
        Fw_Types
)

### Unit Tests ###
# register_fprime_ut(
#     AUTOCODER_INPUTS
#         "${CMAKE_CURRENT_LIST_DIR}/Helpers.fpp"
#     SOURCES
#         "${CMAKE_CURRENT_LIST_DIR}/test/ut/HelpersTestMain.cpp"
#         "${CMAKE_CURRENT_LIST_DIR}/test/ut/HelpersTester.cpp"
#     DEPENDS
#         STest # For rules-based testing
#     UT_AUTO_HELPERS
# )

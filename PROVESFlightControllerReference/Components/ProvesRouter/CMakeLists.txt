####
# F Prime CMakeLists.txt:
#
# SOURCES: list of source files (to be compiled)
# AUTOCODER_INPUTS: list of files to be passed to the autocoders
#
# More information in the F´ CMake API documentation:
# https://fprime.jpl.nasa.gov/latest/docs/reference/api/cmake/API/
#
####

register_fprime_module(
    AUTOCODER_INPUTS
        "${CMAKE_CURRENT_LIST_DIR}/ProvesRouter.fpp"
    SOURCES
        "${CMAKE_CURRENT_LIST_DIR}/ProvesRouter.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/Bypasser.cpp"
)

### Unit Tests ###
register_fprime_ut(
    AUTOCODER_INPUTS
        "${CMAKE_CURRENT_LIST_DIR}/ProvesRouter.fpp"
    SOURCES
        "${CMAKE_CURRENT_LIST_DIR}/test/ut/ProvesRouterTestMain.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/test/ut/ProvesRouterTester.cpp"
    UT_AUTO_HELPERS
)

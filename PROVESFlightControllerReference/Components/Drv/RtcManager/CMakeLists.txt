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

# RtcManager.cpp calls Zephyr APIs; on non-Zephyr platforms register autocode
# only for it, so this module's ports/types stay resolvable to portable
# consumers (see docs/adr/0002). RtcHelper.cpp is portable (no Zephyr
# includes) and builds on every platform.
set(IMPL_SOURCES)
if(FPRIME_PLATFORM STREQUAL "Zephyr")
    set(IMPL_SOURCES "${CMAKE_CURRENT_LIST_DIR}/RtcManager.cpp")
endif()

register_fprime_library(
    AUTOCODER_INPUTS
        "${CMAKE_CURRENT_LIST_DIR}/RtcManager.fpp"
    SOURCES
        "${CMAKE_CURRENT_LIST_DIR}/RtcHelper.cpp"
        ${IMPL_SOURCES}
)

### Unit Tests ###
# register_fprime_ut(
#     AUTOCODER_INPUTS
#         "${CMAKE_CURRENT_LIST_DIR}/RtcManager.fpp"
#     SOURCES
#         "${CMAKE_CURRENT_LIST_DIR}/test/ut/RtcManagerTestMain.cpp"
#         "${CMAKE_CURRENT_LIST_DIR}/test/ut/RtcManagerTester.cpp"
#     DEPENDS
#         STest # For rules-based testing
#     UT_AUTO_HELPERS
# )

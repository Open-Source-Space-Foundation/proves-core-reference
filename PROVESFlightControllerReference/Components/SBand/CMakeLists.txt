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
        "${CMAKE_CURRENT_LIST_DIR}/FprimeHal.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/SBandFaultPolicy.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/RadioLibSBandRadio.cpp"
    DEPENDS
        RadioLib
)

add_subdirectory("${FPRIME_PROJECT_ROOT}/lib/RadioLib" "${CMAKE_BINARY_DIR}/RadioLib")

# Disable compile warnings for RadioLib as they pollute the log and we can't do anything about them.
target_compile_options(RadioLib PRIVATE -w)

# RadioLib defaults to dynamic (heap) allocation; flight code must not
# heap-allocate after boot (S-BAND-REINTEGRATION-PLAN.md root cause #3).
# PUBLIC so it also applies to any TU that includes RadioLib headers
# (SBand.cpp, FprimeHal.cpp, RadioLibSBandRadio.cpp) via the RadioLib
# target dependency above, not just RadioLib's own compilation units.
target_compile_definitions(RadioLib PUBLIC RADIOLIB_STATIC_ONLY=1)

### Unit Tests ###
# F Prime component-level UTs are intentionally not used in this repo: host
# unit tests live under test/unit-tests/ (see test_SBand_FaultPolicy.cpp),
# driving the pure-logic SBandFaultPolicy class directly with no Zephyr/F'
# autocode dependency. The SBand component itself is a thin adapter over
# SBandRadioIf/SBandFaultPolicy, verified by this firmware build and by
# HWIL in PR 3.
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

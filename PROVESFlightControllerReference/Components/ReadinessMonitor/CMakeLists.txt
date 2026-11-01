register_fprime_library(
    AUTOCODER_INPUTS
        "${CMAKE_CURRENT_LIST_DIR}/ReadinessMonitor.fpp"
    SOURCES
        "${CMAKE_CURRENT_LIST_DIR}/ReadinessMonitor.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/ReadinessChecks.cpp"
)

### Unit Tests ###
register_fprime_ut(
    AUTOCODER_INPUTS
        "${CMAKE_CURRENT_LIST_DIR}/ReadinessMonitor.fpp"
    SOURCES
        "${CMAKE_CURRENT_LIST_DIR}/test/ut/ReadinessMonitorTestMain.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/test/ut/ReadinessMonitorTester.cpp"
    UT_AUTO_HELPERS
)

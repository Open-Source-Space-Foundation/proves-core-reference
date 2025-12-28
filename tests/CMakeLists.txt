cmake_minimum_required(VERSION 3.20)
project(proves_unit_tests CXX)

enable_testing()

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../lib/fprime/googletest googletest-build)

# DetumbleManager StrategySelector Tests
add_library(detumble_manager_strategy_selector STATIC
    ${CMAKE_CURRENT_SOURCE_DIR}/../FprimeZephyrReference/Components/DetumbleManager/StrategySelector.cpp
)
target_include_directories(detumble_manager_strategy_selector PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)
add_executable(test_detumble_manager_strategy_selector test_DetumbleManager_StrategySelector.cpp)
target_link_libraries(test_detumble_manager_strategy_selector
    gtest_main
    detumble_manager_strategy_selector
)
add_test(NAME DetumbleManagerStrategySelectorTests COMMAND test_detumble_manager_strategy_selector)

# RtcManager RtcHelper Tests
add_library(rtc_manager_rtc_helper STATIC
    ${CMAKE_CURRENT_SOURCE_DIR}/../FprimeZephyrReference/Components/Drv/RtcManager/RtcHelper.cpp
)
target_include_directories(rtc_manager_rtc_helper PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)
add_executable(test_rtc_manager_rtc_helper test_RtcManager_RtcHelper.cpp)
target_link_libraries(test_rtc_manager_rtc_helper
    gtest_main
    rtc_manager_rtc_helper
)
add_test(NAME RtcManagerRtcHelperTests COMMAND test_rtc_manager_rtc_helper)

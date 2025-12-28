cmake_minimum_required(VERSION 3.20)
project(proves_unit_tests CXX)

enable_testing()

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../lib/fprime/googletest googletest-build)

add_library(rtc_helper STATIC
    ${CMAKE_CURRENT_SOURCE_DIR}/../FprimeZephyrReference/Components/Drv/RtcManager/RtcHelper.cpp
)
target_include_directories(rtc_helper PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

add_executable(test_rtc_helper test_rtc_helper.cpp)
target_link_libraries(test_rtc_helper
    gtest_main
    rtc_helper
)
add_test(NAME RtcHelperTests COMMAND test_rtc_helper)

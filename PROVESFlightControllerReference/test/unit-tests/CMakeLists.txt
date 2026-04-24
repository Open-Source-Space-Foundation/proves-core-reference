cmake_minimum_required(VERSION 3.20)
project(proves_unit_tests CXX)

enable_testing()

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../../lib/fprime/googletest googletest-build)

# --- Helper Libraries ---

# DetumbleManager Magnetorquer
add_library(detumble_manager_magnetorquer STATIC
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../PROVESFlightControllerReference/Components/DetumbleManager/Magnetorquer.cpp
)
target_include_directories(detumble_manager_magnetorquer PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/../../..
)

# DetumbleManager StrategySelector
add_library(detumble_manager_strategy_selector STATIC
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../PROVESFlightControllerReference/Components/DetumbleManager/StrategySelector.cpp
)
target_include_directories(detumble_manager_strategy_selector PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/../../..
)

# RtcManager RtcHelper
add_library(rtc_manager_rtc_helper STATIC
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../PROVESFlightControllerReference/Components/Drv/RtcManager/RtcHelper.cpp
)
target_include_directories(rtc_manager_rtc_helper PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/../../..
)

# PacketProcessor Parser
add_library(packet_processor STATIC
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../PROVESFlightControllerReference/Components/PacketProcessor/Parser.cpp
)
target_include_directories(packet_processor PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/../../..
)

# PacketProcessor Validator
add_library(packet_validator STATIC
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../PROVESFlightControllerReference/Components/PacketProcessor/Validator.cpp
)
target_include_directories(packet_validator PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/../../..
)

# PacketProcessor Authenticator (link against system crypto provider)
add_library(packet_authenticator STATIC
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../PROVESFlightControllerReference/Components/PacketProcessor/Authenticator.cpp
)
target_include_directories(packet_authenticator PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/../../..
    ${CMAKE_CURRENT_SOURCE_DIR} # for AuthDefaultKey.h
)

# Find Mbed TLS (we use libmbedcrypto for PSA provider)
find_path(MBEDTLS_MD_H mbedtls/md.h)
find_path(PSA_CRYPTO_H psa/crypto.h)
find_library(MBEDCRYPTO_LIB mbedcrypto)

if(NOT MBEDCRYPTO_LIB)
    message(FATAL_ERROR "mbedcrypto library not found; install mbedtls (brew install mbedtls) or provide library in LIBRARY_PATH")
endif()

if(MBEDTLS_MD_H)
    target_include_directories(packet_authenticator PUBLIC ${MBEDTLS_MD_H})
endif()

if(PSA_CRYPTO_H)
    target_include_directories(packet_authenticator PUBLIC ${PSA_CRYPTO_H})
endif()

target_link_libraries(packet_authenticator PUBLIC ${MBEDCRYPTO_LIB})

# DetumbleManager BDot
add_library(detumble_manager_bdot STATIC
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../PROVESFlightControllerReference/Components/DetumbleManager/BDot.cpp
)
target_include_directories(detumble_manager_bdot PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/../../..
)

# --- Auto-discover and build tests ---

file(GLOB TEST_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/test_*.cpp")

foreach(test_src ${TEST_SOURCES})
    get_filename_component(test_name ${test_src} NAME_WE)

    add_executable(${test_name} ${test_src})
    target_link_libraries(${test_name}
        gtest_main
        detumble_manager_magnetorquer
        detumble_manager_strategy_selector
        detumble_manager_bdot
        rtc_manager_rtc_helper
        packet_processor
        packet_validator
        packet_authenticator
    )

    add_test(NAME ${test_name} COMMAND ${test_name})
endforeach()

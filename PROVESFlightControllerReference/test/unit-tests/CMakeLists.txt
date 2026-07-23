cmake_minimum_required(VERSION 3.20)
project(proves_unit_tests CXX)

enable_testing()

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../../lib/fprime/googletest googletest-build)

# --- UspRadio: LinkProfiles host-side test (Phase 2) ---
# LinkProfiles.hpp is header-only and free of Zephyr/USP includes, so it
# compiles here with a minimal FPrimeBasicTypes stub provided inline in the test.
add_executable(test_LinkProfiles
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../lib/fprime-zephyr/fprime-zephyr/Drv/UspRadio/test/ut/test_LinkProfiles.cpp
)
target_include_directories(test_LinkProfiles PRIVATE
    # Exposes #include "fprime-zephyr/Drv/UspRadio/LinkProfiles.hpp"
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../lib/fprime-zephyr
)
target_link_libraries(test_LinkProfiles gtest_main)
add_test(NAME test_LinkProfiles COMMAND test_LinkProfiles)

# --- UspRadio: ProfilePolicy host-side test (Phase 3) ---
# ProfilePolicy.cpp is free of F'/USP/Zephyr includes (host-compilable).
# LINK_PROFILES_USE_HOST_TYPES is defined inside the test source to activate
# cstdint typedefs in both ProfilePolicy.hpp and LinkProfiles.hpp.
add_executable(test_ProfilePolicy
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../lib/fprime-zephyr/fprime-zephyr/Drv/UspRadio/test/ut/test_ProfilePolicy.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../lib/fprime-zephyr/fprime-zephyr/Drv/UspRadio/ProfilePolicy.cpp
)
target_include_directories(test_ProfilePolicy PRIVATE
    # Exposes #include "fprime-zephyr/Drv/UspRadio/ProfilePolicy.hpp"
    # and    #include "fprime-zephyr/Drv/UspRadio/LinkProfiles.hpp"
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../lib/fprime-zephyr
)
# ProfilePolicy.cpp includes ProfilePolicy.hpp which includes LinkProfiles.hpp.
# Neither header pulls in F', Zephyr, or USP, so no additional link deps.
target_compile_definitions(test_ProfilePolicy PRIVATE LINK_PROFILES_USE_HOST_TYPES)
target_link_libraries(test_ProfilePolicy gtest_main)
add_test(NAME test_ProfilePolicy COMMAND test_ProfilePolicy)

# --- UspRadio: RadioHeadShim host-side test (RadioHead-compat toggle) ---
# RadioHeadShim.hpp is header-only and free of F'/USP/Zephyr includes.
add_executable(test_RadioHeadShim
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../lib/fprime-zephyr/fprime-zephyr/Drv/UspRadio/test/ut/test_RadioHeadShim.cpp
)
target_include_directories(test_RadioHeadShim PRIVATE
    # Exposes #include "fprime-zephyr/Drv/UspRadio/RadioHeadShim.hpp"
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../lib/fprime-zephyr
)
target_compile_definitions(test_RadioHeadShim PRIVATE LINK_PROFILES_USE_HOST_TYPES)
target_link_libraries(test_RadioHeadShim gtest_main)
add_test(NAME test_RadioHeadShim COMMAND test_RadioHeadShim)

# --- Helper Libraries ---

# DetumbleManager BDot
add_library(detumble_manager_bdot STATIC
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../PROVESFlightControllerReference/Components/DetumbleManager/BDot.cpp
)
target_include_directories(detumble_manager_bdot PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/../../..
)

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

# TcSecurityDeframer Parser
add_library(security_deframer_parser STATIC
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../PROVESFlightControllerReference/Components/TcSecurityDeframer/Parser.cpp
)
target_include_directories(security_deframer_parser PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/../../..
)

# TcSecurityDeframer Validator
add_library(security_deframer_validator STATIC
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../PROVESFlightControllerReference/Components/TcSecurityDeframer/Validator.cpp
)
target_include_directories(security_deframer_validator PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/../../..
)

# TcSecurityDeframer Authenticator
add_library(security_deframer_authenticator STATIC
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../PROVESFlightControllerReference/Components/TcSecurityDeframer/Authenticator.cpp
)
target_include_directories(security_deframer_authenticator PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/../../..
    ${CMAKE_CURRENT_SOURCE_DIR} # for AuthDefaultKey.h
)

# RtcManager RtcHelper
add_library(rtc_manager_rtc_helper STATIC
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../PROVESFlightControllerReference/Components/Drv/RtcManager/RtcHelper.cpp
)
target_include_directories(rtc_manager_rtc_helper PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/../../..
)

# ProvesRouter Bypasser
add_library(proves_router_bypasser STATIC
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../PROVESFlightControllerReference/Components/ProvesRouter/Bypasser.cpp
)
target_include_directories(proves_router_bypasser PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/../../..
)

# Find PSA provider (we use libmbedcrypto) and ensure PSA headers exist
find_path(PSA_CRYPTO_H psa/crypto.h)
find_library(MBEDCRYPTO_LIB mbedcrypto)

if(NOT PSA_CRYPTO_H)
    message(FATAL_ERROR "psa/crypto.h not found; install Mbed TLS PSA provider headers or set CMAKE_INCLUDE_PATH to the directory containing psa/crypto.h")
endif()

if(NOT MBEDCRYPTO_LIB)
    message(FATAL_ERROR "mbedcrypto library not found; install mbedtls (brew install mbedtls) or provide library in LIBRARY_PATH")
endif()

target_include_directories(security_deframer_authenticator PUBLIC ${PSA_CRYPTO_H})
target_link_libraries(security_deframer_authenticator PUBLIC ${MBEDCRYPTO_LIB})

# --- Auto-discover and build tests ---

file(GLOB TEST_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/test_*.cpp")

foreach(test_src ${TEST_SOURCES})
    get_filename_component(test_name ${test_src} NAME_WE)

    add_executable(${test_name} ${test_src})
    target_link_libraries(${test_name}
        gtest_main
        detumble_manager_bdot
        detumble_manager_magnetorquer
        detumble_manager_strategy_selector
        security_deframer_parser
        security_deframer_validator
        security_deframer_authenticator
        rtc_manager_rtc_helper
        proves_router_bypasser
    )

    add_test(NAME ${test_name} COMMAND ${test_name})
endforeach()

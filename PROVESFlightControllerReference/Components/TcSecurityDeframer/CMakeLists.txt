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

# The Zephyr build gets PSA crypto from the Zephyr mbedTLS module; native builds
# use a host mbedTLS install (same requirement as test/unit-tests).
if(FPRIME_PLATFORM STREQUAL "Zephyr")
    set(TCSEC_PLATFORM_DEPENDS kernel)
else()
    set(TCSEC_PLATFORM_DEPENDS)
endif()

# On non-Zephyr platforms, locate a host mbedTLS 3.x (brew install mbedtls /
# apt install libmbedtls-dev) before registering the implementation. When it is
# missing or too old, fall back to autocode-only registration (matching the
# other Zephyr-guarded components, see docs/adr/0002) so the native lane still
# configures; native TcSecurityDeframer compilation is lost in that case.
set(TCSEC_HAVE_HOST_MBEDTLS TRUE)
if(NOT FPRIME_PLATFORM STREQUAL "Zephyr")
    find_path(MBEDTLS_INCLUDE_DIR psa/crypto.h PATHS /opt/homebrew/include /usr/local/include /usr/include)
    find_library(MBEDCRYPTO_LIBRARY mbedcrypto PATHS /opt/homebrew/lib /usr/local/lib /usr/lib)
    if(NOT MBEDTLS_INCLUDE_DIR OR NOT MBEDCRYPTO_LIBRARY)
        set(TCSEC_HAVE_HOST_MBEDTLS FALSE)
        message(WARNING
            "Host mbedTLS not found; registering TcSecurityDeframer autocode only "
            "(native TcSecurityDeframer compilation is lost). Install mbedTLS 3.x "
            "(brew install mbedtls / apt install libmbedtls-dev) to build it natively.")
    elseif(NOT EXISTS "${MBEDTLS_INCLUDE_DIR}/mbedtls/build_info.h")
        # mbedTLS 2.x lacks mbedtls/build_info.h; the implementation needs 3.x.
        set(TCSEC_HAVE_HOST_MBEDTLS FALSE)
        message(WARNING
            "Host mbedTLS at ${MBEDTLS_INCLUDE_DIR} is too old (mbedtls/build_info.h "
            "missing, so it predates 3.x); registering TcSecurityDeframer autocode only "
            "(native TcSecurityDeframer compilation is lost). Install mbedTLS 3.x to "
            "build it natively.")
    endif()
endif()

if(FPRIME_PLATFORM STREQUAL "Zephyr" OR TCSEC_HAVE_HOST_MBEDTLS)
    register_fprime_library(
        AUTOCODER_INPUTS
            "${CMAKE_CURRENT_LIST_DIR}/TcSecurityDeframer.fpp"
        SOURCES
            "${CMAKE_CURRENT_LIST_DIR}/Authenticator.cpp"
            "${CMAKE_CURRENT_LIST_DIR}/TcSecurityDeframer.cpp"
            "${CMAKE_CURRENT_LIST_DIR}/Parser.cpp"
            "${CMAKE_CURRENT_LIST_DIR}/Validator.cpp"
        DEPENDS
            ${TCSEC_PLATFORM_DEPENDS}
            FprimeExtras_Utilities_FileHelper
    )
else()
    register_fprime_library(
        AUTOCODER_INPUTS
            "${CMAKE_CURRENT_LIST_DIR}/TcSecurityDeframer.fpp"
    )
endif()

# Add mbedTLS include directories for PSA crypto headers
# F Prime creates targets based on path: PROVESFlightControllerReference_Components_TcSecurityDeframer
set(PACKET_PARSER_TARGET "PROVESFlightControllerReference_Components_TcSecurityDeframer")

# Add mbedTLS include path - check multiple possible locations
if(NOT FPRIME_PLATFORM STREQUAL "Zephyr")
    if(TCSEC_HAVE_HOST_MBEDTLS)
        target_include_directories(${PACKET_PARSER_TARGET} PRIVATE "${MBEDTLS_INCLUDE_DIR}")
        target_link_libraries(${PACKET_PARSER_TARGET} PRIVATE "${MBEDCRYPTO_LIBRARY}")
    endif()
elseif(DEFINED ZEPHYR_MBEDTLS_MODULE_DIR)
    target_include_directories(${PACKET_PARSER_TARGET} PRIVATE
        "${ZEPHYR_MBEDTLS_MODULE_DIR}/include"
    )
elseif(DEFINED ZEPHYR_BASE)
    # Use ZEPHYR_BASE which is set by find_package(Zephyr)
    target_include_directories(${PACKET_PARSER_TARGET} PRIVATE
        "${ZEPHYR_BASE}/../modules/crypto/mbedtls/include"
    )
else()
    # Final fallback: use relative path from project root
    target_include_directories(${PACKET_PARSER_TARGET} PRIVATE
        "${CMAKE_SOURCE_DIR}/lib/zephyr-workspace/modules/crypto/mbedtls/include"
    )
endif()

### Unit Tests ###
# register_fprime_ut(
#     AUTOCODER_INPUTS
#         "${CMAKE_CURRENT_LIST_DIR}/TcSecurityDeframer.fpp"
#     SOURCES
#         "${CMAKE_CURRENT_LIST_DIR}/test/ut/TcSecurityDeframerTestMain.cpp"
#         "${CMAKE_CURRENT_LIST_DIR}/test/ut/TcSecurityDeframerTester.cpp"
#     DEPENDS
#         STest # For rules-based testing
#     UT_AUTO_HELPERS
# )

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
        "${CMAKE_CURRENT_LIST_DIR}/Authenticate.fpp"
    SOURCES
        "${CMAKE_CURRENT_LIST_DIR}/Authenticate.cpp"
    DEPENDS
        kernel
)

# Add mbedTLS include directories for PSA crypto headers
# F Prime creates targets based on path: FprimeZephyrReference_Components_Authenticate
set(AUTHENTICATE_TARGET "FprimeZephyrReference_Components_Authenticate")

# Add mbedTLS include path - check multiple possible locations
if(DEFINED ZEPHYR_MBEDTLS_MODULE_DIR)
    target_include_directories(${AUTHENTICATE_TARGET} PRIVATE
        "${ZEPHYR_MBEDTLS_MODULE_DIR}/include"
    )
elseif(DEFINED ZEPHYR_BASE)
    # Use ZEPHYR_BASE which is set by find_package(Zephyr)
    target_include_directories(${AUTHENTICATE_TARGET} PRIVATE
        "${ZEPHYR_BASE}/../modules/crypto/mbedtls/include"
    )
else()
    # Final fallback: use relative path from project root
    target_include_directories(${AUTHENTICATE_TARGET} PRIVATE
        "${CMAKE_SOURCE_DIR}/lib/zephyr-workspace/modules/crypto/mbedtls/include"
    )
endif()

### Unit Tests ###
# register_fprime_ut(
#     AUTOCODER_INPUTS
#         "${CMAKE_CURRENT_LIST_DIR}/Authenticate.fpp"
#     SOURCES
#         "${CMAKE_CURRENT_LIST_DIR}/test/ut/AuthenticateTestMain.cpp"
#         "${CMAKE_CURRENT_LIST_DIR}/test/ut/AuthenticateTester.cpp"
#     DEPENDS
#         STest # For rules-based testing
#     UT_AUTO_HELPERS
# )

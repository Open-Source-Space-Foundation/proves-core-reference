####
# Native (host) build for F Prime Component UTs and SIL deployments.
#
# This is a separate build entry from the root CMakeLists.txt, which is
# Zephyr-only (find_package(Zephyr), board config). Zephyr-bound components
# register autocode-only under this build (see docs/adr/0002); portable
# components build fully and may register UTs.
####

cmake_minimum_required(VERSION 3.24.2)

project(proves-native C CXX)

###
# F' Core Setup
###
include("${CMAKE_CURRENT_LIST_DIR}/../lib/fprime/cmake/FPrime.cmake")
fprime_setup_included_code()

# Project-wide components (config overrides + Components)
add_fprime_subdirectory("${CMAKE_CURRENT_LIST_DIR}/../PROVESFlightControllerReference")

# Framework UT helper that fails a handle-size static assert on Darwin; framework
# UTs are disabled, nothing we build depends on it.
if(TARGET Svc_OsTime_test_RawTimeTester)
    set_target_properties(Svc_OsTime_test_RawTimeTester PROPERTIES EXCLUDE_FROM_ALL TRUE)
endif()

# UpdateStatus.fpp is CONFIGURATION_OVERRIDE'd by project/config, leaving the
# extras module with no objects; macOS ar refuses to create an empty archive, so
# give the target one stub object.
if(TARGET FprimeExtras_Update_UpdateStatus)
    target_sources(FprimeExtras_Update_UpdateStatus PRIVATE "${CMAKE_CURRENT_LIST_DIR}/EmptyArchiveStub.cpp")
endif()

# This CMake file is intended to register project-wide objects.
# This allows for reuse between deployments, or other projects.

# Keep me first
add_fprime_subdirectory("${CMAKE_CURRENT_LIST_DIR}/project/config")
add_fprime_subdirectory("${CMAKE_CURRENT_LIST_DIR}/Components")

# Comms subtopologies and the deployment reference fprime-zephyr driver instances,
# which exist only in the Zephyr build (the native build is for component UTs;
# see docs/adr/0002).
if(FPRIME_PLATFORM STREQUAL "Zephyr")
    add_fprime_subdirectory("${CMAKE_CURRENT_LIST_DIR}/ComCcsdsUart/")
    add_fprime_subdirectory("${CMAKE_CURRENT_LIST_DIR}/ComCcsdsSband/")
    add_fprime_subdirectory("${CMAKE_CURRENT_LIST_DIR}/ComCcsdsLora/")
    add_fprime_subdirectory("${CMAKE_CURRENT_LIST_DIR}/ReferenceDeployment/")
endif()

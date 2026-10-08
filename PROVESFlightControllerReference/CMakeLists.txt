# This CMake file is intended to register project-wide objects.
# This allows for reuse between deployments, or other projects.

# Keep me first
add_fprime_subdirectory("${CMAKE_CURRENT_LIST_DIR}/project/config")
add_fprime_subdirectory("${CMAKE_CURRENT_LIST_DIR}/Components")
add_fprime_subdirectory("${CMAKE_CURRENT_LIST_DIR}/ComCcsdsUart/")
add_fprime_subdirectory("${CMAKE_CURRENT_LIST_DIR}/ComCcsdsSband/")
add_fprime_subdirectory("${CMAKE_CURRENT_LIST_DIR}/ComCcsdsLora/")

# One deployment per board. The flight controller is the topology from main.
# The Nicla Vision keeps its own deployment and ID range.
if(BOARD MATCHES "arduino_nicla_vision")
    add_fprime_subdirectory("${CMAKE_CURRENT_LIST_DIR}/NiclaDeployment/")
elseif(BOARD MATCHES "proves_flight_control_board")
    add_fprime_subdirectory("${CMAKE_CURRENT_LIST_DIR}/ReferenceDeployment/")
else()
    message(FATAL_ERROR "BOARD '${BOARD}' does not match a PROVES deployment")
endif()

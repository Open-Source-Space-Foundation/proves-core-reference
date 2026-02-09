#####
# 'ReferenceDeployment' Deployment:
#
# This registers the 'ReferenceDeployment' deployment to the build system.
# Custom components that have not been added at the project-level should be added to
# the list below.
#
#####

###
# Topology and Components
###

add_fprime_subdirectory("${CMAKE_CURRENT_LIST_DIR}/Top/")

if (FPRIME_PLATFORM STREQUAL "Zephyr")
   # Use Zephyr stub for PSRAM; SparkFun sfe_psram.c requires Pico SDK (hardware/*) not available in Zephyr
   register_fprime_zephyr_deployment(
            ${FPRIME_CURRENT_MODULE}
        SOURCES
            "${CMAKE_CURRENT_LIST_DIR}/Main.cpp"
            "${CMAKE_CURRENT_LIST_DIR}/sfe_psram_zephyr_stub.cpp"
        DEPENDS
            ${FPRIME_CURRENT_MODULE}_Top
    )
endif()

# Copyright (c) 2025 BroncoSpace / PROVES Team
# SPDX-License-Identifier: Apache-2.0
#
# In the fprime-zephyr build, this CMakeLists.txt is processed as part of the
# Zephyr "application mode" CMake run (after find_package(Zephyr) has set up
# the kernel). In that context zephyr_library() creates a library that is NOT
# automatically linked to the Zephyr kernel targets.
#
# The correct approach is:
#   - add the driver source to the Zephyr `app` target directly, OR
#   - add it to zephyr_interface so the kernel sees it.
#
# We use target_sources(app ...) which is the canonical Zephyr application
# pattern for adding sources to the application target.

if(CONFIG_MEMC_RP2350_PSRAM)
    # The RP2350 HAL register/struct headers are exposed via INTERFACE targets
    # by the hal_rpi_pico module.  Add paths explicitly — same pattern used in
    # the root CMakeLists.txt for hardware_adc.
    set(HAL_RPI_PICO_SRC
        "${CMAKE_SOURCE_DIR}/lib/zephyr-workspace/modules/hal/rpi_pico/src"
    )
    zephyr_include_directories(
        "${HAL_RPI_PICO_SRC}/rp2350/hardware_structs/include"
        "${HAL_RPI_PICO_SRC}/rp2350/hardware_regs/include"
    )

    # Add driver source to the Zephyr `app` target so it is compiled into the
    # kernel binary and the DEVICE_DT_DEFINE / SYS_INIT entries are included
    # in the linker iterable sections.
    target_sources(app PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}/memc_rp2350_psram.c
    )
endif()

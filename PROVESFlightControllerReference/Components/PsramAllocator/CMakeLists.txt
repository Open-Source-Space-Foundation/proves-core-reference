####
# F Prime CMakeLists.txt — PsramAllocator (issue #285)
#
# Fw::MemAllocator adapter over the PSRAM shared_multi_heap pool.
# Not an F´ component (no .fpp); a plain library for topology setup code.
####

register_fprime_library(
    SOURCES
        "${CMAKE_CURRENT_LIST_DIR}/PsramAllocator.cpp"
    HEADERS
        "${CMAKE_CURRENT_LIST_DIR}/PsramAllocator.hpp"
    DEPENDS
        Fw_Types
)

# When the PSRAM driver is enabled (v5d), PsramAllocator.cpp includes
# "drivers/memc/memc_rp2350_psram.h" from the repo root (same pattern as
# PsramMonitor).
set(PSRAM_ALLOCATOR_TARGET "PROVESFlightControllerReference_Components_PsramAllocator")
if(TARGET ${PSRAM_ALLOCATOR_TARGET})
    target_include_directories(${PSRAM_ALLOCATOR_TARGET} PRIVATE
        "${CMAKE_SOURCE_DIR}"
    )
endif()

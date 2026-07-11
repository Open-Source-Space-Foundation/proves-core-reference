// ======================================================================
// \title  PsramAllocator.cpp
// \brief  Fw::MemAllocator backed by the external PSRAM shared_multi_heap
// ======================================================================

#include "PROVESFlightControllerReference/Components/PsramAllocator/PsramAllocator.hpp"

#ifdef CONFIG_MEMC_RP2350_PSRAM
#include "drivers/memc/memc_rp2350_psram.h"
#include <zephyr/multi_heap/shared_multi_heap.h>
#endif

namespace PROVES {

void* PsramAllocator::allocate(const FwEnumStoreType identifier,
                               FwSizeType& size,
                               bool& recoverable,
                               FwSizeType alignment) {
    (void)identifier;
    recoverable = false;
#ifdef CONFIG_MEMC_RP2350_PSRAM
    if (psram_heap_ok()) {
        void* ptr = shared_multi_heap_aligned_alloc(SMH_REG_ATTR_EXTERNAL, alignment, size);
        if (ptr != nullptr) {
            return ptr;
        }
    }
#else
    (void)alignment;
#endif
    size = 0;
    return nullptr;
}

void PsramAllocator::deallocate(const FwEnumStoreType identifier, void* ptr) {
    (void)identifier;
#ifdef CONFIG_MEMC_RP2350_PSRAM
    if (ptr != nullptr) {
        shared_multi_heap_free(ptr);
    }
#else
    (void)ptr;
#endif
}

}  // namespace PROVES

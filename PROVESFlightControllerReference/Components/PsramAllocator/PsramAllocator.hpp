// ======================================================================
// \title  PsramAllocator.hpp
// \brief  Fw::MemAllocator backed by the external PSRAM shared_multi_heap
//
// Adapter that lets F´ topology code place large, opt-in allocations
// (file uplink staging, framing buffers, payload buffers) in the 2 MB
// APS1604M PSRAM instead of internal SRAM (issue #285).
//
// Usage (topology setup, follow-up PR wires actual consumers):
//   static PROVES::PsramAllocator psramAllocator;
//   someBufferManager.setup(id, 0, psramAllocator, bins);
//
// Behavior when the PSRAM is absent or failed its boot self-test:
// allocate() returns nullptr with size set to 0.  Callers (e.g.
// Svc::BufferManager) treat this as an allocation failure; the choice
// to fall back to an SRAM allocator or to reduce buffer counts is a
// topology decision, deliberately not made inside this adapter.
// ======================================================================

#ifndef PROVES_PSRAM_ALLOCATOR_HPP
#define PROVES_PSRAM_ALLOCATOR_HPP

#include "Fw/Types/MemAllocator.hpp"

namespace PROVES {

class PsramAllocator : public Fw::MemAllocator {
  public:
    PsramAllocator() = default;
    ~PsramAllocator() override = default;

    //! Allocate from the PSRAM shared_multi_heap pool
    //! (SMH_REG_ATTR_EXTERNAL). Returns nullptr and size=0 if the PSRAM
    //! is not present, failed its self-test, or the pool is exhausted.
    //! Memory is not zeroed; recoverable is always false (PSRAM contents
    //! do not survive reboot).
    void* allocate(const FwEnumStoreType identifier,
                   FwSizeType& size,
                   bool& recoverable,
                   FwSizeType alignment = alignof(std::max_align_t)) override;

    //! Return memory to the PSRAM pool. ptr must come from allocate().
    void deallocate(const FwEnumStoreType identifier, void* ptr) override;
};

}  // namespace PROVES

#endif  // PROVES_PSRAM_ALLOCATOR_HPP

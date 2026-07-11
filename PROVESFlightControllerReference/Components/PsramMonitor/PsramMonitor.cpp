// ======================================================================
// \title  PsramMonitor.cpp
// \brief  F' passive component that monitors the RP2350 QMI PSRAM.
//         Issue #285, slice S6.
//
// PsramFree design decision
// -------------------------
// Zephyr v4.3 shared_multi_heap has no "free bytes" query API.  An
// atomic counter maintained outside the driver is not possible without
// intercepting every alloc/free call.  Binary-search probe-alloc/free
// (find largest allocatable block) would work but only makes sense during
// PSRAM_SELF_TEST, not on every 1 Hz tick (fragmentation + thrash risk).
//
// Decision: PsramFree is omitted from the rate-group telemetry path.
// The self-test already confirms the heap is functional.  If a free-space
// estimate is needed in future, add a binary-search probe inside
// PSRAM_SELF_TEST_cmdHandler bounded to ~20 iterations.
//
// Self-test length cap
// --------------------
// PSRAM_SELF_TEST clamps length to SELF_TEST_LENGTH_CAP (256 KB) per
// invocation so the command dispatcher is not blocked for more than ~10 ms.
// For a full 2 MB test the ground operator issues multiple commands with
// consecutive start_offset values.
//
// The test is word-wise save/test/restore, so it is non-destructive to heap
// contents at rest.  Callers that share the tested region concurrently can
// still race the one-word test window; run over quiescent regions when
// possible.  Full-region destructive coverage (address-decode faults) is
// provided by the driver's boot memtest before the heap exists.
// ======================================================================

#include "PROVESFlightControllerReference/Components/PsramMonitor/PsramMonitor.hpp"

// Guard all PSRAM driver calls: the driver symbols only exist when
// CONFIG_MEMC_RP2350_PSRAM is set (v5d).  On v5e the component builds and
// runs but reports status=0 (absent) and size=0.
#ifdef CONFIG_MEMC_RP2350_PSRAM
#include "drivers/memc/memc_rp2350_psram.h"
#endif

namespace Components {

// ----------------------------------------------------------------------
// Construction and destruction
// ----------------------------------------------------------------------

PsramMonitor::PsramMonitor(const char* const compName) : PsramMonitorComponentBase(compName) {}

PsramMonitor::~PsramMonitor() {}

// ----------------------------------------------------------------------
// run_handler — 1 Hz rate group
// ----------------------------------------------------------------------

void PsramMonitor::run_handler(FwIndexType portNum, U32 context) {
#ifdef CONFIG_MEMC_RP2350_PSRAM
    // Cheap read-only calls; safe to call on every tick.
    const bool ready = psram_is_ready();
    const bool heap = psram_heap_ok();
    const U32 sz = static_cast<U32>(psram_size());

    // PsramStatus: 0=absent/failed, 1=mapped, 2=heap-verified
    U8 status = 0U;

    if (ready && heap) {
        status = 2U;
    } else if (ready) {
        status = 1U;
    }

    this->tlmWrite_PsramSize(sz);
    this->tlmWrite_PsramStatus(status);
#else
    // No PSRAM driver compiled in (v5e build); report absent.
    this->tlmWrite_PsramSize(0U);
    this->tlmWrite_PsramStatus(0U);
#endif
}

// ----------------------------------------------------------------------
// PSRAM_SELF_TEST command handler
// ----------------------------------------------------------------------

void PsramMonitor::PSRAM_SELF_TEST_cmdHandler(FwOpcodeType opCode, U32 cmdSeqNum, U32 start_offset, U32 length) {
#ifdef CONFIG_MEMC_RP2350_PSRAM
    if (!psram_is_ready()) {
        this->log_WARNING_LO_PsramNotReady();
        this->cmdResponse_out(opCode, cmdSeqNum, Fw::CmdResponse::EXECUTION_ERROR);
        return;
    }

    // Clamp length to the per-invocation cap.
    if (length > SELF_TEST_LENGTH_CAP) {
        length = SELF_TEST_LENGTH_CAP;
    }

    const uintptr_t base = psram_base();
    const U32 region_size = static_cast<U32>(psram_size());

    // Clamp [start_offset, start_offset+length) to the valid region.
    if (start_offset >= region_size) {
        this->cmdResponse_out(opCode, cmdSeqNum, Fw::CmdResponse::VALIDATION_ERROR);
        return;
    }
    // 32-bit word access requires 4-byte alignment.
    if ((start_offset % sizeof(U32)) != 0U) {
        this->cmdResponse_out(opCode, cmdSeqNum, Fw::CmdResponse::VALIDATION_ERROR);
        return;
    }
    if (start_offset + length > region_size) {
        length = region_size - start_offset;
    }
    // Round down to whole 32-bit words; reject if nothing testable remains.
    length &= ~static_cast<U32>(sizeof(U32) - 1U);
    if (length == 0U) {
        this->cmdResponse_out(opCode, cmdSeqNum, Fw::CmdResponse::VALIDATION_ERROR);
        return;
    }

    // Test through the uncached, non-allocating alias so every access hits
    // the chip instead of the write-back XIP cache.  The alias base comes
    // from the driver (0x15000000 on RP2350 CS1) — do not hardcode it here.
    volatile U32* nc = reinterpret_cast<volatile U32*>(psram_nocache_base() + start_offset);
    const U32 words = length / sizeof(U32);

    // Word-wise save/test/restore: each word is saved, tested with the
    // address-in-address pattern and its inverse, then restored.  This keeps
    // the command safe to run over regions holding live heap allocations
    // (contents are preserved; only a single word is in flux at any time).
    // Full-region destructive address-decode coverage is provided by the
    // driver's boot memtest, which runs before the heap exists.
    for (U32 i = 0U; i < words; i++) {
        const U32 orig = nc[i];

        nc[i] = i;
        if (nc[i] != i) {
            nc[i] = orig;
            this->log_WARNING_HI_PsramSelfTestFailed(start_offset + i * static_cast<U32>(sizeof(U32)), i,
                                                     static_cast<U32>(nc[i]));
            this->cmdResponse_out(opCode, cmdSeqNum, Fw::CmdResponse::EXECUTION_ERROR);
            return;
        }

        nc[i] = ~i;
        if (nc[i] != ~i) {
            nc[i] = orig;
            this->log_WARNING_HI_PsramSelfTestFailed(start_offset + i * static_cast<U32>(sizeof(U32)), ~i,
                                                     static_cast<U32>(nc[i]));
            this->cmdResponse_out(opCode, cmdSeqNum, Fw::CmdResponse::EXECUTION_ERROR);
            return;
        }

        nc[i] = orig;
    }

    this->log_ACTIVITY_LO_PsramSelfTestPassed(start_offset, length);
    this->cmdResponse_out(opCode, cmdSeqNum, Fw::CmdResponse::OK);

#else  // !CONFIG_MEMC_RP2350_PSRAM
    // PSRAM driver not compiled in: always not ready.
    this->log_WARNING_LO_PsramNotReady();
    this->cmdResponse_out(opCode, cmdSeqNum, Fw::CmdResponse::EXECUTION_ERROR);
#endif
}

}  // namespace Components

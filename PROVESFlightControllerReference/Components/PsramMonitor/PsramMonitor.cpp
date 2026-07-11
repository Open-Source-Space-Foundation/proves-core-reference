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
// invocation so the command dispatcher is not blocked for more than ~10 ms
// at 150 MHz (2 passes × 256 KB / (32-bit word at ~75 MHz effective) ≈ 3 ms
// for writes + reads, conservative estimate).  For a full 2 MB test the
// ground operator issues multiple commands with consecutive start_offset
// values.
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
    if (start_offset + length > region_size) {
        length = region_size - start_offset;
    }
    if (length == 0U) {
        this->cmdResponse_out(opCode, cmdSeqNum, Fw::CmdResponse::VALIDATION_ERROR);
        return;
    }

    // Use the uncached, non-allocating alias (PSRAM_NOCACHE_BASE = XIP
    // CS1 nocache window: XIP_NOCACHE_NOALLOC_BASE + 0x01000000).
    // The driver defines this at file scope; reconstruct it here to avoid
    // pulling in HAL register headers in this component.
    //
    // RP2350 datasheet §12.14.4.2: cached XIP window base = 0x11000000,
    // nocache+noalloc alias base = 0x13000000 + 0x01000000 = 0x14000000.
    // The driver's own PSRAM_NOCACHE_BASE uses XIP_NOCACHE_NOALLOC_BASE
    // from hardware/regs/addressmap.h which resolves to 0x13000000; adding
    // 0x01000000 gives the CS1 nocache alias at 0x14000000.
    static constexpr uintptr_t NOCACHE_ALIAS_BASE = 0x14000000U;

    volatile U32* nc = reinterpret_cast<volatile U32*>(NOCACHE_ALIAS_BASE + start_offset);
    const U32 words = length / sizeof(U32);

    // Pass 1: address-in-address pattern (write then verify).
    for (U32 i = 0U; i < words; i++) {
        nc[i] = i;
    }
    for (U32 i = 0U; i < words; i++) {
        if (nc[i] != i) {
            this->log_WARNING_HI_PsramSelfTestFailed(start_offset + i * static_cast<U32>(sizeof(U32)), i,
                                                     static_cast<U32>(nc[i]));
            this->cmdResponse_out(opCode, cmdSeqNum, Fw::CmdResponse::EXECUTION_ERROR);
            return;
        }
    }

    // Pass 2: inverted pattern.
    for (U32 i = 0U; i < words; i++) {
        nc[i] = ~i;
    }
    for (U32 i = 0U; i < words; i++) {
        if (nc[i] != ~i) {
            this->log_WARNING_HI_PsramSelfTestFailed(start_offset + i * static_cast<U32>(sizeof(U32)), ~i,
                                                     static_cast<U32>(nc[i]));
            this->cmdResponse_out(opCode, cmdSeqNum, Fw::CmdResponse::EXECUTION_ERROR);
            return;
        }
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

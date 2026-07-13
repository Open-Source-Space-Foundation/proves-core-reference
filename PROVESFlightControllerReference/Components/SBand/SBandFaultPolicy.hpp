// ======================================================================
// \title  SBandFaultPolicy.hpp
// \brief  hpp file for SBandFaultPolicy pure-logic class
// ======================================================================

#pragma once

#include <cstdint>

namespace Components {

//! Pure logic core implementing the S-Band fault-management state machine
//! (S-BAND-REINTEGRATION-PLAN.md decision D3). Host-compilable: no Zephyr or
//! F Prime autocode dependencies, matching the pattern used by
//! DetumbleManager::BDot and StackMonitorCore. All state is fixed-size;
//! zero heap allocation.
//!
//! Invariant: S-Band failure degrades to "no S-Band", never to backpressure,
//! hang, or spacecraft reset. The component feeds this class the outcome of
//! every radio operation and queries decision() to learn what, if anything,
//! it should do in response.
class SBandFaultPolicy {
  public:
    //! Number of consecutive operation failures that triggers a nRST reset
    //! request. Tunable later; chosen to tolerate a handful of transient
    //! SPI/RF glitches without masking a real fault.
    static constexpr std::uint32_t CONSECUTIVE_FAILURE_LIMIT = 5;  // N

    //! Number of resets performed without an intervening success that
    //! latches FAULTED. Tunable later; chosen so a truly wedged radio stops
    //! consuming reset attempts (and the nRST line) after a bounded number
    //! of tries.
    static constexpr std::uint32_t RESET_ATTEMPT_LIMIT = 3;  // M

    //! Decision the component should act on, as of the most recent event.
    enum class Decision {
        NONE,           //!< Nothing to do; keep operating normally.
        REQUEST_RESET,  //!< Consecutive failures crossed the limit; perform a nRST reset.
        FAULTED         //!< Latched: stop all radio-interface calls until ground intervenes.
    };

    //! Construct a fresh, healthy policy.
    SBandFaultPolicy();

    //! Report that a radio operation succeeded. Clears the consecutive-
    //! failure count and the reset-without-success count. No-op once
    //! latched FAULTED.
    void operationSucceeded();

    //! Report that a radio operation failed. Advances the consecutive-
    //! failure count; once it reaches CONSECUTIVE_FAILURE_LIMIT, decision()
    //! reports REQUEST_RESET until resetCompleted() is called (further
    //! failures in the meantime do not issue additional requests). No-op
    //! once latched FAULTED.
    void operationFailed();

    //! Report that the component finished a nRST reset attempt (regardless
    //! of whether the following re-init succeeds -- that outcome arrives via
    //! a later operationSucceeded()/operationFailed()). Clears the pending
    //! reset request and the consecutive-failure count, and advances the
    //! reset-without-success count; latches FAULTED once that count reaches
    //! RESET_ATTEMPT_LIMIT. No-op once latched FAULTED.
    void resetCompleted();

    //! Ground has requested a reset (e.g. the RESET_RADIO command). Re-arms
    //! the policy to a clean state -- including clearing a FAULTED latch --
    //! so a re-init can be attempted. Unconditional: safe to call even when
    //! not FAULTED.
    void groundResetRequested();

    //! The current decision. Cheap, side-effect-free; safe to query as
    //! often as needed.
    Decision decision() const;

    //! Current consecutive-failure count (for telemetry/EVR args only; not
    //! part of the state-machine's semantics).
    std::uint32_t consecutiveFailures() const;

    //! Current resets-since-last-success count (for telemetry/EVR args
    //! only; not part of the state-machine's semantics).
    std::uint32_t resetsSinceSuccess() const;

  private:
    std::uint32_t m_consecutiveFailures;
    std::uint32_t m_resetsSinceSuccess;
    bool m_resetPending;  //!< A REQUEST_RESET has been issued and not yet completed.
    bool m_faulted;       //!< Latched fault state.
};

}  // namespace Components

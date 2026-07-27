// ======================================================================
// \title  ComDelay.cpp
// \author starchmd
// \brief  cpp file for ComDelay component implementation class
// ======================================================================

#include "PROVESFlightControllerReference/Components/ComDelay/ComDelay.hpp"

namespace Components {

// ----------------------------------------------------------------------
// Component construction and destruction
// ----------------------------------------------------------------------

ComDelay ::ComDelay(const char* const compName) : ComDelayComponentBase(compName) {}

ComDelay ::~ComDelay() {}

void ComDelay ::parameterUpdated(FwPrmIdType id) {
    switch (id) {
        case ComDelay::PARAMID_DIVIDER: {
            Fw::ParamValid is_valid;
            U16 new_divider = this->paramGet_DIVIDER(is_valid);
            if ((is_valid != Fw::ParamValid::INVALID) && (is_valid != Fw::ParamValid::UNINIT)) {
                this->log_ACTIVITY_HI_DividerSet(new_divider);
            }
        } break;
        default:
            FW_ASSERT(0);
            break;  // Fallthrough from assert (static analysis)
    }
}

// ----------------------------------------------------------------------
// Handler implementations for typed input ports
// ----------------------------------------------------------------------

// Image-currency marker: grep-able via `strings zephyr.elf` to verify the flashed
// binary actually contains the divider-0 passthrough change (never trust "Verified OK").
static volatile const char COMDELAY_DIV0_PASSTHROUGH_MARKER[] = "comdelay-div0-passthrough-20260723";

void ComDelay ::comStatusIn_handler(FwIndexType portNum, Fw::Success& condition) {
    // Read the divider; on invalid/uninit fall back to the default (matches run_handler).
    Fw::ParamValid is_valid;
    U16 current_divisor = this->paramGet_DIVIDER(is_valid);
    bool divider_valid = (is_valid != Fw::ParamValid::INVALID) && (is_valid != Fw::ParamValid::UNINIT);

    // Delegate the latch-vs-passthrough decision to the extracted state machine:
    // when the (effective) divider is 0, the status is forwarded immediately and
    // never latched, so downlink is paced purely by radio TX-done. DIVIDER > 0
    // keeps the latched/tick-paced behavior.
    //
    // Threading note: ComDelay is passive, so a passthrough forward executes on the
    // CALLER's thread (the radio-side comStatus source). That is safe because
    // comStatusOut feeds an async input (ComQueue), which only enqueues a message.
    // In passthrough mode the latch valid flag is never set, so this status cannot
    // ALSO be emitted by run_handler (no duplication); a status latched earlier under
    // DIVIDER > 0 is still consumed by the tick's compare_exchange (no loss) if the
    // divider is changed to 0 at runtime.
    bool forward_bit = false;
    if (this->m_logic.acceptStatus(condition == Fw::Success::SUCCESS, current_divisor, divider_valid, forward_bit)) {
        static_cast<void>(COMDELAY_DIV0_PASSTHROUGH_MARKER[0]);  // volatile read keeps the marker in the image
        Fw::Success forwarded = forward_bit ? Fw::Success::SUCCESS : Fw::Success::FAILURE;
        this->comStatusOut_out(0, forwarded);
    }
}

void ComDelay ::run_handler(FwIndexType portNum, U32 context) {
    // Unless there is corruption, the parameter should always be valid via its default value; however, in the interest
    // of failing-safe and continuing some sort of communication we default the current_divisor to the default value.
    Fw::ParamValid is_valid;
    U16 current_divisor = this->paramGet_DIVIDER(is_valid);
    bool divider_valid = (is_valid != Fw::ParamValid::INVALID) && (is_valid != Fw::ParamValid::UNINIT);

    // Delegate to the extracted, host-testable tick/divider/latch state machine. This preserves the exact
    // pre-existing behavior (including the U8 tick-counter width): on the cycle the counter is at 0, attempt
    // to consume (exactly once) any latched status and emit it, then advance/reset the counter against the
    // current divisor (or the default divisor, if the parameter is not currently valid).
    bool status_bit = false;
    bool should_emit = this->m_logic.tick(current_divisor, divider_valid, status_bit);
    if (should_emit) {
        Fw::Success condition = status_bit ? Fw::Success::SUCCESS : Fw::Success::FAILURE;
        this->comStatusOut_out(0, condition);
    }
}

}  // namespace Components

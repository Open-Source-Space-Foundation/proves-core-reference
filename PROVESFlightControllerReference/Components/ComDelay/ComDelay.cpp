// ======================================================================
// \title  ComDelay.cpp
// \author starchmd
// \brief  cpp file for ComDelay component implementation class
// ======================================================================

#include "PROVESFlightControllerReference/Components/ComDelay/ComDelay.hpp"

#include "PROVESFlightControllerReference/Components/ComDelay/FppConstantsAc.hpp"

namespace Components {

// ----------------------------------------------------------------------
// Component construction and destruction
// ----------------------------------------------------------------------

ComDelay ::ComDelay(const char* const compName)
    : ComDelayComponentBase(compName), m_last_status_valid(false), m_last_status(Fw::Success::FAILURE) {}

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
    if ((is_valid == Fw::ParamValid::INVALID) || (is_valid == Fw::ParamValid::UNINIT)) {
        current_divisor = Components::DEFAULT_DIVIDER;
    }

    if (current_divisor == 0) {
        // DIVIDER == 0 means "no delay": forward the status immediately instead of
        // latching it for the next rate tick. This removes the rate-group quantization
        // (one status per tick) so downlink is paced purely by radio TX-done.
        //
        // Threading note: ComDelay is passive, so this executes on the CALLER's thread
        // (the radio-side comStatus source). That is safe because comStatusOut feeds an
        // async input (ComQueue), which only enqueues a message here.
        //
        // Coherence with run_handler: in passthrough mode we never set
        // m_last_status_valid, so this status cannot ALSO be emitted by run_handler
        // (no duplication). A status latched earlier under DIVIDER > 0 is still
        // consumed by run_handler's compare_exchange as before (no loss) if the
        // divider is changed to 0 at runtime.
        static_cast<void>(COMDELAY_DIV0_PASSTHROUGH_MARKER[0]);  // volatile read keeps the marker in the image
        this->comStatusOut_out(0, condition);
    } else {
        this->m_last_status = condition;
        this->m_last_status_valid = true;
    }
}

void ComDelay ::run_handler(FwIndexType portNum, U32 context) {
    // On the cycle after the tick count is reset, attempt to output any current com status
    if (this->m_tick_count == 0) {
        bool expected = true;
        // Receive the current "last status" validity flag and atomically exchange it with false. This effectively
        // "consumes" a valid status.  When valid, the last status is sent out.
        bool valid = this->m_last_status_valid.compare_exchange_strong(expected, false);
        if (valid) {
            this->comStatusOut_out(0, this->m_last_status);
        }
    }

    // Unless there is corruption, the parameter should always be valid via its default value; however, in the interest
    // of failing-safe and continuing some sort of communication we default the current_divisor to the default value.
    Fw::ParamValid is_valid;
    U16 current_divisor = this->paramGet_DIVIDER(is_valid);

    // Increment and module the tick count by the divisor
    if ((is_valid == Fw::ParamValid::INVALID) || (is_valid == Fw::ParamValid::UNINIT)) {
        current_divisor = Components::DEFAULT_DIVIDER;
    }
    // Count this new tick, resetting whenever the current count is at or higher than the current divider.
    this->m_tick_count = (this->m_tick_count >= current_divisor) ? 0 : this->m_tick_count + 1;
}

}  // namespace Components

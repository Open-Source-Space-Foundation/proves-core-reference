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

void ComDelay ::comStatusIn_handler(FwIndexType portNum, Fw::Success& condition) {
    this->m_logic.latchStatus(condition == Fw::Success::SUCCESS);
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

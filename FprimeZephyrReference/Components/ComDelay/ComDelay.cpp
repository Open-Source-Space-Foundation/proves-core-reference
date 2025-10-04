// ======================================================================
// \title  ComDelay.cpp
// \author starchmd
// \brief  cpp file for ComDelay component implementation class
// ======================================================================

#include "FprimeZephyrReference/Components/ComDelay/ComDelay.hpp"
#include "FprimeZephyrReference/Components/ComDelay/FppConstantsAc.hpp"

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
            U8 new_divider = this->paramGet_DIVIDER(is_valid);
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
    this->m_last_status = condition;
    this->m_last_status_valid = true;
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
    U8 current_divisor = this->paramGet_DIVIDER(is_valid);

    // Increment and module the tick count by the divisor
    if ((is_valid == Fw::ParamValid::INVALID) || (is_valid == Fw::ParamValid::UNINIT)) {
        current_divisor = Components::DEFAULT_DIVIDER;
    }
    // Count this new tick, resetting whenever the current count is at or higher than the current divider.
    this->m_tick_count = (this->m_tick_count >= current_divisor) ? 0 : this->m_tick_count + 1;
}

}  // namespace Components

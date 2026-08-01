// ======================================================================
// \title  ComDelayLogic.hpp
// \brief  hpp file for ComDelayLogic class
//
// Host-testable extraction of the ComDelay tick/divider/latch state
// machine. This class holds no F' or Zephyr dependencies so it can be
// compiled and unit tested directly on the host (see
// PROVESFlightControllerReference/test/unit-tests/test_ComDelay_ComDelayLogic.cpp).
//
// The ComDelay component (ComDelay.cpp) delegates to this class; the
// behavior here must remain identical to what the component previously
// implemented inline.
// ======================================================================

#pragma once

#include <atomic>
#include <cstdint>

namespace Components {

//! Default divider value: on a 1Hz input tick, this releases a latched
//! status roughly every 30s (299 + 1 ticks).
constexpr std::uint16_t COM_DELAY_DEFAULT_DIVIDER = 299;

class ComDelayLogic {
  public:
    ComDelayLogic() : m_tick_count(0), m_last_status_valid(false), m_last_status(false) {}

    ~ComDelayLogic() = default;

    //! Latch an incoming status, overwriting any status not yet consumed.
    void latchStatus(bool status) {
        this->m_last_status = status;
        this->m_last_status_valid = true;
    }

    //! Accept an incoming status. When the effective divider (or the default
    //! divider, if `dividerValid` is false) is 0, the status is NOT latched:
    //! it is returned for immediate forwarding (`outStatus`) and the latch
    //! valid flag is never set, so it cannot also be emitted by a later tick.
    //! When the effective divider is > 0, the status is latched exactly as
    //! latchStatus() does and false is returned.
    //!
    //! Returns true if the status should be forwarded immediately (passthrough).
    bool acceptStatus(bool status, std::uint16_t divider, bool dividerValid, bool& outStatus) {
        std::uint16_t current_divisor = dividerValid ? divider : COM_DELAY_DEFAULT_DIVIDER;
        if (current_divisor == 0) {
            outStatus = status;
            return true;
        }
        this->latchStatus(status);
        return false;
    }

    //! Advance one tick. If the internal counter is currently at 0, attempt to
    //! consume (exactly once) any latched status and report it for emission.
    //! The counter is then advanced/reset against `divider` (or the default
    //! divider, if `dividerValid` is false).
    //!
    //! Returns true if a latched status should be emitted this tick, and
    //! writes the value to consume into `outStatus`.
    bool tick(std::uint16_t divider, bool dividerValid, bool& outStatus) {
        bool shouldEmit = false;
        if (this->m_tick_count == 0) {
            bool expected = true;
            // Atomically consume the latched status flag, mirroring the
            // production compare_exchange_strong "consume once" semantics.
            bool valid = this->m_last_status_valid.compare_exchange_strong(expected, false);
            if (valid) {
                outStatus = this->m_last_status;
                shouldEmit = true;
            }
        }

        std::uint16_t current_divisor = dividerValid ? divider : COM_DELAY_DEFAULT_DIVIDER;
        this->m_tick_count = (this->m_tick_count >= current_divisor) ? 0 : this->m_tick_count + 1;

        return shouldEmit;
    }

    //! Test/introspection helper: current tick counter value.
    std::uint8_t tickCount() const { return this->m_tick_count; }

    //! Test/introspection helper: whether a status is currently latched.
    bool hasLatchedStatus() const { return this->m_last_status_valid; }

  private:
    //! Count of incoming run ticks
    std::uint8_t m_tick_count;
    //! Stores if the last status is currently valid (not yet consumed)
    std::atomic<bool> m_last_status_valid;
    //! Stores the last latched status
    bool m_last_status;
};

}  // namespace Components

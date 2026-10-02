// ======================================================================
// \title  RtcHelper.cpp
// \brief  cpp file for RtcHelper class
// ======================================================================

#include "RtcHelper.hpp"

namespace Drv {

// ----------------------------------------------------------------------
// Component construction and destruction
// ----------------------------------------------------------------------

RtcHelper ::RtcHelper()
    : m_initialized(false), m_last_seen_seconds(0), m_useconds_offset(0), m_last_raw_useconds(0), m_wrap_count(0) {}

RtcHelper ::~RtcHelper() {}

// ----------------------------------------------------------------------
// Public helper methods
// ----------------------------------------------------------------------

RescaledTime RtcHelper ::rescaleUseconds(std::uint32_t current_seconds, std::uint32_t current_useconds) {
    // Start a new bucket on the first call, or if a new RTC second has been observed. current_useconds
    // may cycle past 1,000,000 multiple times while current_seconds is unchanged (the RTC only updates
    // once per real second, or slower if the read lags), so wraps are tracked cumulatively against the
    // raw value seen on the previous call rather than corrected once against the fixed offset.
    if (!this->m_initialized || this->m_last_seen_seconds != current_seconds) {
        this->m_initialized = true;
        this->m_last_seen_seconds = current_seconds;
        this->m_useconds_offset = current_useconds;
        this->m_last_raw_useconds = current_useconds;
        this->m_wrap_count = 0;
    } else if (current_useconds < this->m_last_raw_useconds) {
        this->m_wrap_count++;
    }
    this->m_last_raw_useconds = current_useconds;

    // Total elapsed useconds since m_useconds_offset was set, including any full cycles counted above.
    const std::uint32_t elapsed = (current_useconds + this->m_wrap_count * 1000000) - this->m_useconds_offset;

    // FPrime expects microseconds in the range [0, 999999]. Any whole seconds in elapsed overflowed
    // out of the useconds field and must be reported back so the caller can carry them into seconds.
    return RescaledTime{elapsed % 1000000, elapsed / 1000000};
}

}  // namespace Drv

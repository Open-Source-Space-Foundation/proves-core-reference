// ======================================================================
// \title  RtcHelper.cpp
// \brief  cpp file for RtcHelper class
// ======================================================================

#include "RtcHelper.hpp"

namespace Drv {

// ----------------------------------------------------------------------
// Component construction and destruction
// ----------------------------------------------------------------------

RtcHelper ::RtcHelper() {}

RtcHelper ::~RtcHelper() {}

// ----------------------------------------------------------------------
// Public helper methods
// ----------------------------------------------------------------------

std::uint32_t RtcHelper ::rescaleUseconds(std::uint32_t current_seconds, std::uint32_t current_useconds) {
    // Update offset if a new second has been observed
    if (this->m_last_seen_seconds != current_seconds) {
        this->m_last_seen_seconds = current_seconds;
        this->m_useconds_offset = current_useconds;
    }

    // Handle wrap around
    if (current_useconds < this->m_useconds_offset) {
        current_useconds += 1000000;
    }

    // Compute delta
    std::uint32_t delta = current_useconds - this->m_useconds_offset;

    // FPrime expects microseconds in the range [0, 999999]
    return delta % 1000000;
}

}  // namespace Drv

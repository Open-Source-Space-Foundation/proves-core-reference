// ======================================================================
// \title  TimeDiscipline.cpp
// \brief  cpp file for TimeDiscipline class
// ======================================================================

#include "TimeDiscipline.hpp"

namespace Drv {

// ----------------------------------------------------------------------
// Public methods
// ----------------------------------------------------------------------

// STUB: intentionally incomplete for the red phase of TDD. Real behavior
// implemented in a following commit.

void TimeDiscipline ::seed(std::int64_t rtc_s, std::int64_t uptime_us) {
    static_cast<void>(rtc_s);
    static_cast<void>(uptime_us);
}

TimeDiscipline::CorrectionResult TimeDiscipline ::correct(std::int64_t rtc_s, std::int64_t uptime_us_at_edge) {
    static_cast<void>(rtc_s);
    static_cast<void>(uptime_us_at_edge);
    return {Correction::IGNORED, 0};
}

bool TimeDiscipline ::read(std::int64_t uptime_us, std::uint32_t& seconds, std::uint32_t& useconds) {
    static_cast<void>(uptime_us);
    seconds = 0;
    useconds = 0;
    return false;
}

}  // namespace Drv

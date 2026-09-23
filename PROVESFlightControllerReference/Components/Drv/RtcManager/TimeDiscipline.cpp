// ======================================================================
// \title  TimeDiscipline.cpp
// \brief  cpp file for TimeDiscipline class
// ======================================================================

#include "TimeDiscipline.hpp"

namespace Drv {

// ----------------------------------------------------------------------
// Public methods
// ----------------------------------------------------------------------

namespace {
constexpr std::int64_t US_PER_S = 1000000;
}  // namespace

void TimeDiscipline ::seed(std::int64_t rtc_s, std::int64_t uptime_us) {
    this->m_offset_us = (rtc_s * US_PER_S) - uptime_us;
    this->m_last_rtc_s = rtc_s;
    this->m_last_reported_us = 0;
    this->m_backward_count = 0;
    this->m_disciplined = true;
}

TimeDiscipline::CorrectionResult TimeDiscipline ::correct(std::int64_t rtc_s, std::int64_t uptime_us_at_edge) {
    // A never-seeded instance has no valid last_rtc_s to compare against, so the first
    // correction always seeds regardless of rtc_s.
    if (this->m_disciplined && (rtc_s <= this->m_last_rtc_s)) {
        return {Correction::IGNORED, 0};
    }

    this->m_last_rtc_s = rtc_s;

    if (!this->m_disciplined) {
        this->seed(rtc_s, uptime_us_at_edge);
        return {Correction::APPLIED, 0};
    }

    const std::int64_t new_offset_us = (rtc_s * US_PER_S) - uptime_us_at_edge;
    const std::int64_t correction_us = new_offset_us - this->m_offset_us;

    if (correction_us < -STEP_THRESHOLD_US) {
        ++this->m_backward_count;
        if (this->m_backward_count < BACKWARD_STEP_CONFIRMATIONS) {
            return {Correction::REJECTED, correction_us};
        }
        this->m_offset_us = new_offset_us;
        this->m_last_reported_us = 0;
        this->m_backward_count = 0;
        return {Correction::STEPPED, correction_us};
    }

    this->m_backward_count = 0;
    this->m_offset_us = new_offset_us;

    if (correction_us > STEP_THRESHOLD_US) {
        this->m_last_reported_us = 0;
        return {Correction::STEPPED, correction_us};
    }

    return {Correction::APPLIED, correction_us};
}

bool TimeDiscipline ::read(std::int64_t uptime_us, std::uint32_t& seconds, std::uint32_t& useconds) {
    if (!this->m_disciplined) {
        seconds = 0;
        useconds = 0;
        return false;
    }

    std::int64_t candidate_us = uptime_us + this->m_offset_us;
    std::int64_t reported_us = (candidate_us > this->m_last_reported_us) ? candidate_us : this->m_last_reported_us;
    if (reported_us < 0) {
        reported_us = 0;
    }
    this->m_last_reported_us = reported_us;

    seconds = static_cast<std::uint32_t>(reported_us / US_PER_S);
    useconds = static_cast<std::uint32_t>(reported_us % US_PER_S);
    return true;
}

}  // namespace Drv

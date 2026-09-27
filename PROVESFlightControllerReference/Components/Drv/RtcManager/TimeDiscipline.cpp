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

bool TimeDiscipline ::isPlausibleRtcSeconds(std::int64_t rtc_s) {
    return (rtc_s >= RTC_MIN_S) && (rtc_s <= RTC_MAX_S);
}

void TimeDiscipline ::seed(std::int64_t rtc_s, std::int64_t uptime_us) {
    this->m_offset_us = (rtc_s * US_PER_S) - uptime_us;
    this->m_last_rtc_s = rtc_s;
    this->m_last_reported_us = 0;
    this->m_pending_count = 0;
    this->m_disciplined = true;
}

TimeDiscipline::CorrectionResult TimeDiscipline ::correct(std::int64_t rtc_s, std::int64_t uptime_us_at_edge) {
    // A never-seeded instance has no valid last_rtc_s to compare against, so the first
    // correction always seeds regardless of rtc_s.
    if (!this->m_disciplined) {
        this->seed(rtc_s, uptime_us_at_edge);
        return {Correction::APPLIED, 0};
    }

    // m_last_rtc_s changes only on a seed or an applied correction. A rejected sample does not
    // change it, so one bogus far-future sample cannot cause later good samples to be ignored.
    if (rtc_s <= this->m_last_rtc_s) {
        return {Correction::IGNORED, 0};
    }

    const std::int64_t new_offset_us = (rtc_s * US_PER_S) - uptime_us_at_edge;
    const std::int64_t correction_us = new_offset_us - this->m_offset_us;

    if ((correction_us > STEP_THRESHOLD_US) || (correction_us < -STEP_THRESHOLD_US)) {
        // Out of band, in either direction. A step needs STEP_CONFIRMATIONS sequential samples
        // whose corrections agree within STEP_THRESHOLD_US, each with a larger rtc_s than the
        // one before. A late callback or one corrupted RTC read is thus never applied.
        const std::int64_t delta_us = correction_us - this->m_pending_correction_us;
        const bool consistent = (this->m_pending_count > 0) && (rtc_s > this->m_pending_rtc_s) &&
                                (delta_us <= STEP_THRESHOLD_US) && (delta_us >= -STEP_THRESHOLD_US);
        this->m_pending_count = consistent ? (this->m_pending_count + 1) : 1;
        this->m_pending_correction_us = correction_us;
        this->m_pending_rtc_s = rtc_s;
        if (this->m_pending_count < STEP_CONFIRMATIONS) {
            return {Correction::REJECTED, correction_us};
        }
        this->m_offset_us = new_offset_us;
        this->m_last_rtc_s = rtc_s;
        this->m_last_reported_us = 0;
        this->m_pending_count = 0;
        return {Correction::STEPPED, correction_us};
    }

    this->m_pending_count = 0;
    this->m_offset_us = new_offset_us;
    this->m_last_rtc_s = rtc_s;
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

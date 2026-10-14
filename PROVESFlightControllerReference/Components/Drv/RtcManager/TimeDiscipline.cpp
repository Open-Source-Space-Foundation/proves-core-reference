// ======================================================================
// \title  TimeDiscipline.cpp
// \brief  cpp file for TimeDiscipline class
// ======================================================================

#include "TimeDiscipline.hpp"

namespace Drv {

// ----------------------------------------------------------------------
// Public methods
// ----------------------------------------------------------------------

bool TimeDiscipline ::isPlausibleRtcSeconds(std::int64_t rtc_s) {
    return (rtc_s >= RTC_MIN_S) && (rtc_s <= RTC_MAX_S);
}

void TimeDiscipline ::seed(std::int64_t rtc_s, std::int64_t uptime_us) {
    this->m_offset_us = (rtc_s * US_PER_S) - uptime_us;
    this->m_last_rtc_s = rtc_s;
    this->m_last_reported_us = 0;  // Drop the monotonic floor so time can move back once
    this->m_pending_count = 0;
    this->m_seeded = true;
}

TimeDiscipline::CorrectionResult TimeDiscipline ::correct(std::int64_t rtc_s, std::int64_t uptime_us) {
    if (!this->m_seeded) {
        this->seed(rtc_s, uptime_us);
        return {Correction::APPLIED, 0};
    }

    if (rtc_s <= this->m_last_rtc_s) {
        return {Correction::IGNORED, 0};
    }

    const std::int64_t new_offset_us = (rtc_s * US_PER_S) - uptime_us;
    const std::int64_t correction_us = new_offset_us - this->m_offset_us;

    if ((correction_us > STEP_THRESHOLD_US) || (correction_us < -STEP_THRESHOLD_US)) {
        // Large correction: step only if it agrees within STEP_THRESHOLD_US with the previous large one
        const std::int64_t delta_us = correction_us - this->m_pending_correction_us;
        const bool confirmsPending = (this->m_pending_count > 0) && (rtc_s > this->m_pending_rtc_s) &&
                                     (delta_us <= STEP_THRESHOLD_US) && (delta_us >= -STEP_THRESHOLD_US);
        this->m_pending_count = confirmsPending ? (this->m_pending_count + 1) : 1;
        this->m_pending_correction_us = correction_us;
        this->m_pending_rtc_s = rtc_s;
        if (this->m_pending_count < STEP_CONFIRMATIONS) {
            return {Correction::REJECTED, correction_us};
        }
        this->m_offset_us = new_offset_us;
        this->m_last_rtc_s = rtc_s;
        this->m_last_reported_us = 0;  // Drop the monotonic floor so time can move back once
        this->m_pending_count = 0;
        return {Correction::STEPPED, correction_us};
    }

    this->m_pending_count = 0;
    this->m_offset_us = new_offset_us;
    this->m_last_rtc_s = rtc_s;
    return {Correction::APPLIED, correction_us};
}

bool TimeDiscipline ::read(std::int64_t uptime_us, std::uint32_t& seconds, std::uint32_t& useconds) {
    if (!this->m_seeded) {
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

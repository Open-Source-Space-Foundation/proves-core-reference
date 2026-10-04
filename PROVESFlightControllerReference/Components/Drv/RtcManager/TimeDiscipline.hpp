// ======================================================================
// \title  TimeDiscipline.hpp
// \brief  hpp file for TimeDiscipline class
// ======================================================================

#pragma once

#include <cstdint>

namespace Drv {

//! Spacecraft time as uptime plus a time offset, corrected at each RTC second edge. Not thread safe
class TimeDiscipline {
  public:
    // ----------------------------------------------------------------------
    // Constants
    // ----------------------------------------------------------------------

    //! Corrections with a magnitude greater than this many microseconds are steps
    static constexpr std::int64_t STEP_THRESHOLD_US = 100000;

    //! Sequential consistent corrections beyond STEP_THRESHOLD_US needed to apply a step
    static constexpr int STEP_CONFIRMATIONS = 2;

    //! Earliest RTC seconds the RV3028 can represent: 2000-01-01T00:00:00Z
    static constexpr std::int64_t RTC_MIN_S = 946684800;

    //! Latest RTC seconds the RV3028 can represent: 2099-12-31T23:59:59Z
    static constexpr std::int64_t RTC_MAX_S = 4102444799;

    //! Outcome of a call to correct()
    enum class Correction {
        IGNORED,   //!< rtc_s did not increase since the last seed or applied correction
        APPLIED,   //!< Correction applied, magnitude <= STEP_THRESHOLD_US, or first seed
        REJECTED,  //!< Correction beyond STEP_THRESHOLD_US, not yet confirmed
        STEPPED,   //!< Confirmed correction beyond STEP_THRESHOLD_US applied
    };

    //! Result returned by correct()
    struct CorrectionResult {
        Correction kind;             //!< The outcome
        std::int64_t correction_us;  //!< The calculated correction, always populated
    };

    // ----------------------------------------------------------------------
    // Public methods
    // ----------------------------------------------------------------------

    //! Return true if rtc_s is in [RTC_MIN_S, RTC_MAX_S], the range the RV3028 can represent
    static bool isPlausibleRtcSeconds(std::int64_t rtc_s);

    //! Seed the time offset from one RTC read
    void seed(std::int64_t rtc_s,       //!< RTC seconds
              std::int64_t uptime_us);  //!< Uptime in microseconds at the seed

    //! Apply an RTC second-edge correction. Seeds and returns {APPLIED, 0} if not yet seeded
    CorrectionResult correct(std::int64_t rtc_s,               //!< RTC seconds read at the edge
                             std::int64_t uptime_us_at_edge);  //!< Uptime in microseconds at the edge

    //! Read the current disciplined time. Returns false if not yet seeded
    bool read(std::int64_t uptime_us,    //!< Uptime in microseconds
              std::uint32_t& seconds,    //!< Reported seconds, out
              std::uint32_t& useconds);  //!< Reported microseconds, out, always in [0, 999999]

  private:
    // ----------------------------------------------------------------------
    // Private member variables
    // ----------------------------------------------------------------------

    bool m_disciplined = false;                //!< True once a seed has occurred
    std::int64_t m_offset_us = 0;              //!< RTC time minus uptime, in microseconds
    std::int64_t m_last_reported_us = 0;       //!< Last reported time, in microseconds, for monotonicity
    std::int64_t m_last_rtc_s = 0;             //!< RTC seconds of the last seed or applied correction
    int m_pending_count = 0;                   //!< Sequential consistent corrections beyond STEP_THRESHOLD_US
    std::int64_t m_pending_correction_us = 0;  //!< Correction of the last pending step candidate
    std::int64_t m_pending_rtc_s = 0;          //!< RTC seconds of the last pending step candidate
};

}  // namespace Drv

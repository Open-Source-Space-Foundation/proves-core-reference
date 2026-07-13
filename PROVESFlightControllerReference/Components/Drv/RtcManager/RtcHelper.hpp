// ======================================================================
// \title  RtcHelper.hpp
// \brief  hpp file for RtcHelper class
// ======================================================================

#pragma once

#include <cstdint>

namespace Drv {

//! Result of rescaling useconds: the rescaled microseconds, plus any whole seconds that
//! overflowed out of the microseconds field and must be added to the caller's seconds value.
struct RescaledTime {
    std::uint32_t useconds;       //!< The rescaled microseconds, always in [0, 999999]
    std::uint32_t seconds_carry;  //!< Whole seconds that overflowed out of useconds
};

class RtcHelper {
  public:
    // ----------------------------------------------------------------------
    // Component construction and destruction
    // ----------------------------------------------------------------------

    //! Construct RtcHelper object
    RtcHelper();

    //! Destroy RtcHelper object
    ~RtcHelper();

  public:
    // ----------------------------------------------------------------------
    // Public helper methods
    // ----------------------------------------------------------------------

    //! Rescales useconds to ensure monotonic increase because RV3028 does not provide sub-second precision
    //! We have two clocks: the RTC clock which provides seconds, and the system uptime clock which provides
    //! milliseconds since boot. We use the initial offset of the system uptime clock when the RTC time is first read
    //! to ensure that the microseconds portion of the time is always increasing.
    //!
    //! Because current_seconds only advances once per real second (or slower, if the RTC read lags),
    //! current_useconds can cycle past 1,000,000 more than once while current_seconds is unchanged.
    //! Each cycle is tracked so total elapsed time since current_seconds was last observed can exceed
    //! 1,000,000us; the whole-second part is reported via seconds_carry so the caller can add it to
    //! current_seconds instead of silently losing it, which would otherwise make the returned time
    //! appear to go backward.
    RescaledTime rescaleUseconds(
        std::uint32_t current_seconds,  //<! The epoch seconds from the RTC
        std::uint32_t current_useconds  //<! The microseconds since boot from the system uptime clock
    );

  private:
    // ----------------------------------------------------------------------
    // Private member variables
    // ----------------------------------------------------------------------

    bool m_initialized;                 //!< Whether a first sample has been observed yet
    std::uint32_t m_last_seen_seconds;  //!< The last seen seconds value from the RTC
    std::uint32_t m_useconds_offset;    //!< The offset to apply to microseconds to ensure monotonicity
    std::uint32_t m_last_raw_useconds;  //!< The raw useconds seen on the previous call, for wrap detection
    std::uint32_t m_wrap_count;         //!< Cycles of current_useconds observed since m_useconds_offset was set
};

}  // namespace Drv

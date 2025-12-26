// ======================================================================
// \title  RtcHelper.hpp
// \brief  hpp file for RtcHelper class
// ======================================================================

#pragma once

#include <cstdint>

namespace Drv {

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
    std::uint32_t rescaleUseconds(
        std::uint32_t current_seconds,  //<! The epoch seconds from the RTC
        std::uint32_t current_useconds  //<! The microseconds since boot from the system uptime clock
    );

  private:
    // ----------------------------------------------------------------------
    // Private member variables
    // ----------------------------------------------------------------------

    std::uint32_t m_last_seen_seconds;  //!< The last seen seconds value from the RTC
    std::uint32_t m_useconds_offset;    //!< The offset to apply to microseconds to ensure monotonicity
};

}  // namespace Drv

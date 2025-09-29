// ======================================================================
// \title  Rv3028Manager.cpp
// \brief  cpp file for Rv3028Manager component implementation class
// ======================================================================

#include "FprimeZephyrReference/Components/Drv/Rv3028Manager/Rv3028Manager.hpp"

namespace Drv {

// ----------------------------------------------------------------------
// Component construction and destruction
// ----------------------------------------------------------------------

Rv3028Manager ::Rv3028Manager(const char* const compName) : Rv3028ManagerComponentBase(compName) {
    // Initialize device
    this->rv3028 = device_get_binding("RV3028");
}

Rv3028Manager ::~Rv3028Manager() {}

// ----------------------------------------------------------------------
// Handler implementations for commands
// ----------------------------------------------------------------------

void Rv3028Manager ::timeGetPort_handler(FwIndexType portNum, Fw::Time& time) {
    // Check device readiness
    if (!device_is_ready(this->rv3028)) {
        // Use logger instead of events since this fn is in a critical path for FPrime
        // to get time. Events require time, if this method fails an event will fail.
        Fw::Logger::log("RV2038 not ready");
        return;
    }

    // Get time from RTC
    U32 posix_time;
    U32 u_secs;
    this->getTime(posix_time, u_secs);

    // Set FPrime time object
    time.set(TimeBase::TB_WORKSTATION_TIME, 0, posix_time, u_secs);
}

U32 Rv3028Manager ::timeGet_handler(FwIndexType portNum) {
    // Check device readiness
    if (!device_is_ready(this->rv3028)) {
        this->log_WARNING_HI_DeviceNotReady();
        return 0;
    }
    this->log_WARNING_HI_DeviceNotReady_ThrottleClear();

    // Get time from RTC
    U32 posix_time;
    U32 u_secs;
    this->getTime(posix_time, u_secs);

    return posix_time;
}

void Rv3028Manager ::timeSet_handler(FwIndexType portNum, const Drv::TimeData& t) {
    // Check device readiness
    if (!device_is_ready(this->rv3028)) {
        this->log_WARNING_HI_DeviceNotReady();
        return;
    }
    this->log_WARNING_HI_DeviceNotReady_ThrottleClear();

    // Populate rtc_time structure from TimeData
    const struct rtc_time time_rtc = {
        .tm_sec = static_cast<int>(t.get_Second()),
        .tm_min = static_cast<int>(t.get_Minute()),
        .tm_hour = static_cast<int>(t.get_Hour()),
        .tm_mday = static_cast<int>(t.get_Day()),
        .tm_mon = static_cast<int>(t.get_Month() - 1),     // month [0-11]
        .tm_year = static_cast<int>(t.get_Year() - 1900),  // year since 1900
        .tm_wday = 0,
        .tm_yday = 0,
        .tm_isdst = 0,
    };

    // Set time on RTC
    const int status = rtc_set_time(this->rv3028, &time_rtc);

    // Report whether setting the time was successful
    if (status == 0) {
        U32 posix_time;
        U32 u_secs;
        this->getTime(posix_time, u_secs);
        this->log_ACTIVITY_HI_TimeSet(posix_time);
    } else {
        this->log_WARNING_HI_TimeNotSet();
    }
}

// ----------------------------------------------------------------------
// Private helper methods
// ----------------------------------------------------------------------
void Rv3028Manager ::getTime(U32& posix_time, U32& u_secs) {
    // Read time from RTC
    struct rtc_time time_rtc = {};
    rtc_get_time(this->rv3028, &time_rtc);

    // Convert time to POSIX time_t format
    struct tm* time_tm = rtc_time_to_tm(&time_rtc);

    errno = 0;
    time_t time_pt = timeutil_timegm(time_tm);
    if (errno == ERANGE) {
        Fw::Logger::log("RV2038 returned invalid time");
        return;
    }

    // Get microsecond part from system clock cycles
    // Note: RV3028 does not provide sub-second precision, so this is
    // just an approximation based on system cycles.
    // FPrime expects microseconds in the range [0, 999999]
    uint32_t time_usecs = k_cyc_to_us_near32(k_cycle_get_32()) % 1000000;

    // Set output parameters
    posix_time = static_cast<U32>(time_pt);
    u_secs = static_cast<U32>(time_usecs);
}

}  // namespace Drv

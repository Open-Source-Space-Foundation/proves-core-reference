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
    FW_ASSERT(device_is_ready(this->rv3028));
}

Rv3028Manager ::~Rv3028Manager() {}

// ----------------------------------------------------------------------
// Handler implementations for commands
// ----------------------------------------------------------------------

void Rv3028Manager ::timeGetPort_handler(FwIndexType portNum, Fw::Time& time) {
    // Check device readiness
    if (!device_is_ready(this->rv3028)) {
        this->log_WARNING_HI_DeviceNotReady();
        return;
    }

    // Fetch time from RTC
    struct rtc_time time_rtc = {};
    rtc_get_time(this->rv3028, &time_rtc);

    // Convert time to POSIX time_t format
    struct tm* time_tm = rtc_time_to_tm(&time_rtc);
    time_t time_posix = timeutil_timegm(time_tm);

    // Set FPrime time object
    time.set(TimeBase::TB_WORKSTATION_TIME, 0, static_cast<U32>(time_posix), 0);
}

U32 Rv3028Manager ::timeRead_handler(FwIndexType portNum) {
    // Check device readiness
    if (!device_is_ready(this->rv3028)) {
        this->log_WARNING_HI_DeviceNotReady();
        return 0;
    }

    // Fetch time from RTC
    struct rtc_time time_rtc = {};
    rtc_get_time(this->rv3028, &time_rtc);

    // Convert time to POSIX time_t format
    struct tm* time_tm = rtc_time_to_tm(&time_rtc);
    time_t time_posix = timeutil_timegm(time_tm);

    return time_posix;
}

void Rv3028Manager ::timeSet_handler(FwIndexType portNum, const Drv::TimeData& t) {
    // Check device readiness
    if (!device_is_ready(this->rv3028)) {
        this->log_WARNING_HI_DeviceNotReady();
        return;
    }

    // Populate rtc_time structure from TimeData
    const struct rtc_time time_rtc = {
        .tm_sec = 0,
        .tm_min = t.get_Minute(),
        .tm_hour = t.get_Hour(),
        .tm_mday = t.get_Day(),
        .tm_mon = t.get_Month() - 1,     // month [0-11]
        .tm_year = t.get_Year() - 1900,  // year since 1900
        .tm_wday = 0,
        .tm_yday = 0,
        .tm_isdst = 0,
    };

    // Set time on RTC
    const int status = rtc_set_time(this->rv3028, &time_rtc);

    // Report whether setting the time was successful
    if (status == 0) {
        this->log_ACTIVITY_HI_TimeSet();
    } else {
        this->log_WARNING_HI_TimeNotSet();
    }
}

}  // namespace Drv

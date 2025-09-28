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

    I32 posix_time = this->getPosixTime();
    if (posix_time < 0) {
        Fw::Logger::log("getPosixTime returned invalid time");
        return;
    }

    // Set FPrime time object
    time.set(TimeBase::TB_WORKSTATION_TIME, 0, static_cast<U32>(posix_time), 0);
}

U32 Rv3028Manager ::timeGet_handler(FwIndexType portNum) {
    // Check device readiness
    if (!device_is_ready(this->rv3028)) {
        this->log_WARNING_HI_DeviceNotReady();
        return 0;
    }
    this->log_WARNING_HI_DeviceNotReady_ThrottleClear();

    I32 posix_time = this->getPosixTime();
    if (posix_time < 0) {
        this->log_WARNING_HI_InvalidTime();
        return 0;
    }
    this->log_WARNING_HI_InvalidTime_ThrottleClear();

    return static_cast<U32>(posix_time);
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

    // Event current time to correlate prior events when new time is set
    this->log_ACTIVITY_HI_TimeGet(this->getPosixTime());

    // Set time on RTC
    const int status = rtc_set_time(this->rv3028, &time_rtc);

    // Report whether setting the time was successful
    if (status == 0) {
        this->log_ACTIVITY_HI_TimeSet();
    } else {
        this->log_WARNING_HI_TimeNotSet();
    }
}

// ----------------------------------------------------------------------
// Private helper methods
// ----------------------------------------------------------------------
I32 Rv3028Manager ::getPosixTime() {
    struct rtc_time time_rtc = {};
    rtc_get_time(this->rv3028, &time_rtc);

    // Convert time to POSIX time_t format
    struct tm* time_tm = rtc_time_to_tm(&time_rtc);

    errno = 0;
    time_t posix_time = timeutil_timegm(time_tm);
    if (errno == ERANGE) {
        Fw::Logger::log("RV2038 returned invalid time");
        return -1;
    }

    return posix_time;
}

}  // namespace Drv

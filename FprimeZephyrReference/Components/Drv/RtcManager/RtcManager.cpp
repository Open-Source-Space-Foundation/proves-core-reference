// ======================================================================
// \title  RtcManager.cpp
// \brief  cpp file for RtcManager component implementation class
// ======================================================================

#include "FprimeZephyrReference/Components/Drv/RtcManager/RtcManager.hpp"

namespace Drv {

// ----------------------------------------------------------------------
// Component construction and destruction
// ----------------------------------------------------------------------

RtcManager ::RtcManager(const char* const compName) : RtcManagerComponentBase(compName) {
    // Initialize device
    this->dev = device_get_binding("RV3028");
}

RtcManager ::~RtcManager() {}

// ----------------------------------------------------------------------
// Handler implementations for typed input ports
// ----------------------------------------------------------------------

void RtcManager ::timeGetPort_handler(FwIndexType portNum, Fw::Time& time) {
    // Check device readiness
    if (!device_is_ready(this->dev)) {
        // Use logger instead of events since this fn is in a critical path for FPrime
        // to get time. Events require time, if this method fails an event will fail.
        Fw::Logger::log("RTC not ready");
        return;
    }

    // Get time from RTC
    struct rtc_time time_rtc = {};
    rtc_get_time(this->dev, &time_rtc);

    // Convert to generic tm struct
    struct tm* time_tm = rtc_time_to_tm(&time_rtc);

    // Convert to time_t (seconds since epoch)
    errno = 0;
    time_t seconds = timeutil_timegm(time_tm);
    if (errno == ERANGE) {
        Fw::Logger::log("RTC returned invalid time");
        return;
    }

    // Get microseconds from system clock cycles
    // Note: RV3028 does not provide sub-second precision, so this is
    // just an approximation based on system cycles.
    // FPrime expects microseconds in the range [0, 999999]
    uint32_t useconds = k_cyc_to_us_near32(k_cycle_get_32()) % 1000000;

    // Set FPrime time object
    time.set(TimeBase::TB_WORKSTATION_TIME, 0, static_cast<U32>(seconds), static_cast<U32>(useconds));
}

// ----------------------------------------------------------------------
// Handler implementations for commands
// ----------------------------------------------------------------------

void RtcManager ::TIME_SET_cmdHandler(FwOpcodeType opCode, U32 cmdSeq, Drv::TimeData t) {
    // Check device readiness
    if (!device_is_ready(this->dev)) {
        // Emit device not ready event
        this->log_WARNING_HI_DeviceNotReady();

        // Send command response
        this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::EXECUTION_ERROR);
        return;
    }
    this->log_WARNING_HI_DeviceNotReady_ThrottleClear();

    // Validate time data
    if (!this->timeDataIsValid(t)) {
        // Emit time not set event
        this->log_WARNING_HI_TimeNotSet();

        // Send command response
        this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::VALIDATION_ERROR);
        return;
    }

    // Store current time for logging
    Fw::Time time_before_set = this->getTime();

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
    const int status = rtc_set_time(this->dev, &time_rtc);

    if (status != 0) {
        // Emit time not set event
        this->log_WARNING_HI_TimeNotSet();

        // Send command response
        this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::EXECUTION_ERROR);
        return;
    }

    // Emit time set event, include previous time for reference
    this->log_ACTIVITY_HI_TimeSet(time_before_set.getSeconds(), time_before_set.getUSeconds());

    // Send command response
    this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
}

bool RtcManager ::timeDataIsValid(Drv::TimeData t) {
    bool valid = true;

    if (t.get_Year() < 1900) {
        this->log_WARNING_HI_YearValidationFailed(t.get_Year());
        valid = false;
    }

    if (t.get_Month() < 1 || t.get_Month() > 12) {
        this->log_WARNING_HI_MonthValidationFailed(t.get_Month());
        valid = false;
    }

    if (t.get_Day() < 1 || t.get_Day() > 31) {
        this->log_WARNING_HI_DayValidationFailed(t.get_Day());
        valid = false;
    }

    if (t.get_Hour() > 23) {
        this->log_WARNING_HI_HourValidationFailed(t.get_Hour());
        valid = false;
    }

    if (t.get_Minute() > 59) {
        this->log_WARNING_HI_MinuteValidationFailed(t.get_Minute());
        valid = false;
    }

    if (t.get_Second() > 59) {
        this->log_WARNING_HI_SecondValidationFailed(t.get_Second());
        valid = false;
    }

    return valid;
}

}  // namespace Drv

// ======================================================================
// \title  RtcManager.cpp
// \brief  cpp file for RtcManager component implementation class
// ======================================================================

#include "PROVESFlightControllerReference/Components/Drv/RtcManager/RtcManager.hpp"

#include <errno.h>

#include <cstdint>
#include <random>

#include <zephyr/device.h>
#include <zephyr/kernel.h>
#include <zephyr/syscalls/rtc.h>

namespace Drv {

// ----------------------------------------------------------------------
// Component construction and destruction
// ----------------------------------------------------------------------

RtcManager ::RtcManager(const char* const compName) : RtcManagerComponentBase(compName), m_rtcHelper() {
    // alarm mask and time value struct initialization

    // match to timedata DO NOT CHANGE IF YOU ARE SETTING this->curr_mask YOU ARE DOING SOMETHING WRONG!!!
    //this->curr_mask = RTC_ALARM_TIME_MASK_YEAR | RTC_ALARM_TIME_MASK_MONTH | RTC_ALARM_TIME_MASK_MONTHDAY | RTC_ALARM_TIME_MASK_HOUR | RTC_ALARM_TIME_MASK_MINUTE | RTC_ALARM_TIME_MASK_SECOND;
    rtc_alarm_get_supported_fields(this->m_dev, 0, &this->curr_mask);

    // alarm time initialization
    memset(&this->m_alarm_time, 0, sizeof(struct rtc_time));

    //alarm set
    this->alarm_set = false;
}

RtcManager ::~RtcManager() {}

// ----------------------------------------------------------------------
// Public helper methods
// ----------------------------------------------------------------------

void RtcManager ::configure(const struct device* dev) {
    this->m_dev = dev;
}

// ----------------------------------------------------------------------
// Handler implementations for typed input ports
// ----------------------------------------------------------------------

void RtcManager ::timeGetPort_handler(FwIndexType portNum, Fw::Time& time) {
    // Get system uptime
    int64_t t = k_uptime_get();
    U32 seconds_since_boot = static_cast<U32>(t / 1000);
    U32 useconds_since_boot = static_cast<U32>((t % 1000) * 1000);

    // Check device readiness
    if (!device_is_ready(this->m_dev)) {
        // Use logger instead of events since this fn is in a critical path for FPrime
        // to get time. Events require time, if this method fails an event will fail.
        //
        // Throttle this message to prevent console flooding and program delays
        if (!this->m_console_throttled) {
            this->m_console_throttled = true;
            Fw::Logger::log("RTC not ready\n");
        }

        // Use monotonic time as fallback
        time.set(TimeBase::TB_PROC_TIME, 0, seconds_since_boot, useconds_since_boot);
        return;
    }

    // Get time from RTC
    struct rtc_time time_rtc = {};
    rtc_get_time(this->m_dev, &time_rtc);

    // Convert to generic tm struct
    struct tm* time_tm = rtc_time_to_tm(&time_rtc);

    // Convert to time_t (seconds since epoch)
    errno = 0;
    U32 seconds_real_time = static_cast<U32>(timeutil_timegm(time_tm));
    if (errno == ERANGE) {
        Fw::Logger::log("RTC returned invalid time");
        return;
    }

    // Set FPrime time object
    time.set(TimeBase::TB_WORKSTATION_TIME, 0, seconds_real_time,
             this->m_rtcHelper.rescaleUseconds(seconds_real_time, useconds_since_boot));
}

// ----------------------------------------------------------------------
// Handler implementations for commands
// ----------------------------------------------------------------------

void RtcManager ::TIME_SET_cmdHandler(FwOpcodeType opCode, U32 cmdSeq, Drv::TimeData t) {
    // Check device readiness
    if (!device_is_ready(this->m_dev)) {
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

    // Cancel any running sequences
    for (FwIndexType i = 0; i < this->getNum_cancelSequences_OutputPorts(); i++) {
        if (!this->isConnected_cancelSequences_OutputPort(i)) {
            continue;
        }
        this->cancelSequences_out(i);
    }

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
    const int status = rtc_set_time(this->m_dev, &time_rtc);

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

// Alarm manager
void RtcManager ::ALARM_SET_cmdHandler(FwOpcodeType opCode, U32 cmdSeq, Drv::TimeData t) {
    // check if alarm is already present or not if it isn't ..
    // use this->m_dev to refer to the device]
    uint16_t mask = this->curr_mask;
    int alarmPresent = rtc_alarm_get_time(this->m_dev, 0, &mask, &this->m_alarm_time);

    if (alarmPresent != 0 || mask == 0) {
        // populate alarm time
        this->m_alarm_time.tm_sec = t.get_Second();
        this->m_alarm_time.tm_min = t.get_Minute();
        this->m_alarm_time.tm_hour = t.get_Hour();
        this->m_alarm_time.tm_mday = t.get_Day();
        this->m_alarm_time.tm_mon = t.get_Month() - 1;     
        this->m_alarm_time.tm_year = t.get_Year() - 1900;

        int rc = rtc_alarm_set_time(this->m_dev, 0, this->curr_mask, &this->m_alarm_time);
        if (rc != 0) {
            // log failure
            this->log_WARNING_HI_AlarmNotSet(t, rc);
            this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::EXECUTION_ERROR);
            return;
        }

        // log success
        this->alarm_set = true;
        this->log_ACTIVITY_HI_AlarmSet(0, t);
        this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);

    } else {
        // if alarm IS present handle it
        // log failure
        // indicate no return code (user error)
        this->log_WARNING_HI_AlarmNotSet(t, 0);
        this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::EXECUTION_ERROR);
    }
}

void RtcManager ::ALARM_CANCEL_cmdHandler(FwOpcodeType opCode, U32 cmdSeq, U16 ID) {
    // check if it's present
    uint16_t mask = this->curr_mask;
    int rc = rtc_alarm_get_time(this->m_dev, 0, &mask, &this->m_alarm_time);

    if (rc == 0 && this->alarm_set) {
        // set mask to 0 to cancel alarm
        mask = 0;
        rtc_alarm_set_time(this->m_dev, 0, mask, &this->m_alarm_time);

        this->alarm_set = false;
        this->log_ACTIVITY_HI_AlarmCanceled(ID);
    } else {
        // handle no alarm case
        this->log_WARNING_HI_AlarmNotCanceled(ID, 0);
        this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::EXECUTION_ERROR);
    }

    this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
}

void RtcManager ::ALARM_LIST_cmdHandler(FwOpcodeType opCode, U32 cmdSeq) {
    // check if present, if so log its info.
    uint16_t mask = this->curr_mask;
    int rc = rtc_alarm_get_time(this->m_dev, 0, &mask, &this->m_alarm_time);

    if (rc == 0) {
        //check the current mask to see if an alarm is active
        
        if(this->alarm_set){
            // convert alarm time and log it
            Drv::TimeData alarm_time_value(this->m_alarm_time.tm_year + 1900, this->m_alarm_time.tm_mon + 1,
                                        this->m_alarm_time.tm_mday, this->m_alarm_time.tm_hour,
                                        this->m_alarm_time.tm_min, this->m_alarm_time.tm_sec);
            log_ACTIVITY_HI_AlarmSet(0, alarm_time_value);
        } else {
            //no alarms likely needs it's own event but this is okay for now
            Drv::TimeData alarm_none;
            this->log_WARNING_HI_AlarmNotSet(alarm_none, 0);
        }

        this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
    } else {
        //indicates hardware error
        this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::EXECUTION_ERROR);

    }
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

// ======================================================================
// \title  RtcManager.cpp
// \brief  cpp file for RtcManager component implementation class
// ======================================================================

#include "FprimeZephyrReference/Components/RtcManager/RtcManager.hpp"

namespace Components {

// ----------------------------------------------------------------------
// Component construction and destruction
// ----------------------------------------------------------------------

RtcManager ::RtcManager(const char* const compName) : RtcManagerComponentBase(compName) {
    // Initialize device
    this->rv3028 = device_get_binding("RV3028");
    FW_ASSERT(device_is_ready(this->rv3028));
}

RtcManager ::~RtcManager() {}

// ----------------------------------------------------------------------
// Handler implementations for commands
// ----------------------------------------------------------------------

void RtcManager ::SET_TIME_cmdHandler(FwOpcodeType opCode, U32 cmdSeq, Components::RtcManager_TimeInput t) {
    if (!device_is_ready(this->rv3028)) {
        this->log_WARNING_HI_RTC_NotReady();
        return;
    }

    // set the time
    const struct rtc_time timeptr = {
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
    const int status = rtc_set_time(this->rv3028, &timeptr);

    if (status == 0) {
        this->log_ACTIVITY_HI_RTC_Set(true);
    } else {
        this->log_ACTIVITY_HI_RTC_Set(false);
    }

    this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
}

void RtcManager ::GET_TIME_cmdHandler(FwOpcodeType opCode, U32 cmdSeq) {
    if (!device_is_ready(this->rv3028)) {
        this->log_WARNING_HI_RTC_NotReady();
        return;
    }

    struct rtc_time timeptr = {};
    rtc_get_time(this->rv3028, &timeptr);

    this->log_ACTIVITY_HI_RTC_GetTime(timeptr.tm_year + 1900, timeptr.tm_mon + 1, timeptr.tm_mday + 1, timeptr.tm_hour,
                                      timeptr.tm_min, timeptr.tm_sec);

    // 3 parts.
    // 1. fprime-zephyr reference need ZephyrTime SVC + instance/topology
    // 2. RV3028Driver talk to zephyr and set the time on the RTC
    // 3. RtcManager component to take commands and relay them to RV3028Driver and get/set

    // TODO - Need a way to sync Fprime time with RTC time, especially after
    // satellite restarts. RTC has a battery backup and should keep time
    // Fprime time will not know anything about the rtc
    // need some kind of sync mechanism maybe 1Hz sync?

    // Try Fprime Time class
    const Fw::Time fwtime = this->getTime();
    const U32 secs = fwtime.getSeconds();

    this->log_ACTIVITY_HI_RTC_GetTime(0, 0, 0, 0, 0, secs);

    this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
}

void RtcManager ::timeGetPort_handler(FwIndexType portNum, /*!< The port number*/
                                      Fw::Time& time       /*!< The U32 cmd argument*/
) {
    if (!device_is_ready(this->rv3028)) {
        this->log_WARNING_HI_RTC_NotReady();
        return;
    }

    struct rtc_time timeptr = {};
    rtc_get_time(this->rv3028, &timeptr);

    // convert to timespec
    struct tm* tcopy = rtc_time_to_tm(&timeptr);
    time_t tcopy2 = timeutil_timegm(tcopy);

    // struct timespec stime = {0};
    // stime.tv_sec = tcopy2;
    // stime.tv_nsec = timeptr.tm_sec * 1000000000L;

    // timespec stime;
    // (void)clock_gettime(CLOCK_REALTIME, &stime);
    time.set(TimeBase::TB_WORKSTATION_TIME, 0, static_cast<U32>(tcopy2), 0);
    // time.set(TimeBase::TB_WORKSTATION_TIME, 0, static_cast<U32>(stime.tv_sec), static_cast<U32>(stime.tv_nsec /
    // 1000));
}

}  // namespace Components

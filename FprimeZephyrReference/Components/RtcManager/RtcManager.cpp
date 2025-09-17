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

void RtcManager ::SET_TIME_cmdHandler(FwOpcodeType opCode, U32 cmdSeq) {
    if (!device_is_ready(this->rv3028)) {
        this->log_WARNING_HI_RTC_NotReady();
        return;
    }

    // set the time
    const struct rtc_time timeptr = {
        .tm_sec = 0,
        .tm_min = 0,
        .tm_hour = 12,
        .tm_mday = 25,
        .tm_mon = 11,
        .tm_year = 125,  // year since 1900
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

    this->log_ACTIVITY_HI_RTC_GetTime(timeptr.tm_year + 1900, timeptr.tm_mon + 1, timeptr.tm_mday, timeptr.tm_hour,
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

}  // namespace Components

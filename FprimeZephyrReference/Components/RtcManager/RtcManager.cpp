// ======================================================================
// \title  RtcManager.cpp
// \brief  cpp file for RtcManager component implementation class
// ======================================================================

#include "FprimeZephyrReference/Components/RtcManager/RtcManager.hpp"
#include <string>

namespace Components {

// ----------------------------------------------------------------------
// Component construction and destruction
// ----------------------------------------------------------------------

RtcManager ::RtcManager(const char* const compName) : RtcManagerComponentBase(compName) {}

RtcManager ::~RtcManager() {}

// ----------------------------------------------------------------------
// Handler implementations for commands
// ----------------------------------------------------------------------

void RtcManager ::SET_TIME_cmdHandler(FwOpcodeType opCode, U32 cmdSeq, Drv::TimeData t) {
    this->timeSet_out(0, t);
    this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
}

void RtcManager ::GET_TIME_cmdHandler(FwOpcodeType opCode, U32 cmdSeq) {
    U32 t = this->timeGet_out(0);

    // Convert POSIX time to tm struct for human readable logging
    time_t time_posix = static_cast<time_t>(t);
    struct tm time_tm_buf;
    struct tm* time_tm = gmtime_r(&time_posix, &time_tm_buf);

    // Convert to ISO format
    char iso_time[32];
    int result = snprintf(iso_time, sizeof(iso_time), "%04d-%02d-%02dT%02d:%02d:%02d", time_tm->tm_year + 1900,
                          time_tm->tm_mon + 1, time_tm->tm_mday, time_tm->tm_hour, time_tm->tm_min, time_tm->tm_sec);

    // Ensure snprintf succeeded and output was not truncated
    FW_ASSERT(sizeof(iso_time) > static_cast<size_t>(result));

    Fw::String iso_time_str(iso_time);

    // Report time retrieved in ISO format
    this->log_ACTIVITY_HI_GetTime(iso_time_str);

    this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
}

}  // namespace Components

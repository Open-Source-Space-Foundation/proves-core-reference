// ======================================================================
// \title  RtcManager.cpp
// \brief  cpp file for RtcManager component implementation class
// ======================================================================

#include "FprimeZephyrReference/Components/RtcManager/RtcManager.hpp"

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
    U32 t = this->timeRead_out(0);

    // Convert POSIX time to tm struct for human readable logging
    time_t time_posix = static_cast<time_t>(t);
    struct tm time_tm_buf;
    struct tm* time_tm = gmtime_r(&time_posix, &time_tm_buf);

    // Report time retrieved
    this->log_ACTIVITY_HI_GetTime(time_tm->tm_year + 1900, time_tm->tm_mon + 1, time_tm->tm_mday + 1, time_tm->tm_hour,
                                  time_tm->tm_min, time_tm->tm_sec);

    this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
}

}  // namespace Components

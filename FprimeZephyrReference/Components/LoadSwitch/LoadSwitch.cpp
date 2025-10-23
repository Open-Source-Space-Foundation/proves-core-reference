// ======================================================================
// \title  LoadSwitch.cpp
// \author sarah
// \brief  cpp file for LoadSwitch component implementation class
// ======================================================================

#include "FprimeZephyrReference/Components/LoadSwitch/LoadSwitch.hpp"

namespace Components {

// ----------------------------------------------------------------------
// Component construction and destruction
// ----------------------------------------------------------------------

LoadSwitch ::LoadSwitch(const char* const compName) : LoadSwitchComponentBase(compName) {}

LoadSwitch ::~LoadSwitch() {}

// ----------------------------------------------------------------------
// Handler implementations for commands
// ----------------------------------------------------------------------

void LoadSwitch ::TURN_ON_cmdHandler(FwOpcodeType opCode, U32 cmdSeq) {
    // TODO
    this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
}

void LoadSwitch ::TURN_OFF_cmdHandler(FwOpcodeType opCode, U32 cmdSeq) {
    // TODO
    this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
}

}  // namespace Components

// ======================================================================
// \title  CameraHandler.cpp
// \author moises
// \brief  cpp file for CameraHandler component implementation class
// ======================================================================

#include "FprimeZephyrReference/Components/CameraHandler/CameraHandler.hpp"

namespace Components {

// ----------------------------------------------------------------------
// Component construction and destruction
// ----------------------------------------------------------------------

CameraHandler ::CameraHandler(const char* const compName) : CameraHandlerComponentBase(compName) {}

CameraHandler ::~CameraHandler() {}

// ----------------------------------------------------------------------
// Handler implementations for commands
// ----------------------------------------------------------------------

void CameraHandler ::TODO_cmdHandler(FwOpcodeType opCode, U32 cmdSeq) {
    // TODO
    this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
}

}  // namespace Components

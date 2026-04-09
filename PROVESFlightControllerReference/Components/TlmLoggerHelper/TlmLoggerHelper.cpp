// ======================================================================
// \title  TlmLoggerHelper.cpp
// \author aaron
// \brief  cpp file for TlmLoggerHelper component implementation class
// ======================================================================

#include "PROVESFlightControllerReference/Components/TlmLoggerHelper/TlmLoggerHelper.hpp"

namespace ReferenceDeployment {

// ----------------------------------------------------------------------
// Component construction and destruction
// ----------------------------------------------------------------------

TlmLoggerHelper ::TlmLoggerHelper(const char* const compName) : TlmLoggerHelperComponentBase(compName) {}

TlmLoggerHelper ::~TlmLoggerHelper() {}

// ----------------------------------------------------------------------
// Handler implementations for typed input ports
// ----------------------------------------------------------------------

void TlmLoggerHelper ::comIn_handler(FwIndexType portNum, Fw::ComBuffer& data, U32 context) {
    // TODO
}

// ----------------------------------------------------------------------
// Handler implementations for commands
// ----------------------------------------------------------------------

void TlmLoggerHelper ::CONNECT_TLM_LOGGER_cmdHandler(FwOpcodeType opCode, U32 cmdSeq) {
    // TODO
    this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
}

}  // namespace ReferenceDeployment

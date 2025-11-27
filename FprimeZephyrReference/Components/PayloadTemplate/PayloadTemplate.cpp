// ======================================================================
// \title  PayloadTemplate.cpp
// \author robertpendergrast
// \brief  cpp file for PayloadTemplate component implementation class
// ======================================================================

#include "FprimeZephyrReference/Components/PayloadTemplate/PayloadTemplate.hpp"

namespace Components {

// ----------------------------------------------------------------------
// Component construction and destruction
// ----------------------------------------------------------------------

PayloadTemplate ::PayloadTemplate(const char* const compName) : PayloadTemplateComponentBase(compName) {}

PayloadTemplate ::~PayloadTemplate() {}

// ----------------------------------------------------------------------
// Handler implementations for typed input ports
// ----------------------------------------------------------------------

void PayloadTemplate ::dataIn_handler(FwIndexType portNum, Fw::Buffer& buffer, const Drv::ByteStreamStatus& status) {
    // This is a synchronous input port handler that gets called each time the
    // payload com receives data from the UART connection.
    
    // You will need to define a data protocol for your payload.
    // See the CameraHandler component for an example.
}

// ----------------------------------------------------------------------
// Handler implementations for commands
// ----------------------------------------------------------------------


void PayloadTemplate ::SEND_COMMAND_cmdHandler(FwOpcodeType opCode, U32 cmdSeq, const Fw::CmdStringArg& cmd) {
    // Send your command to the payload com component
    
    Fw::CmdStringArg tempCmd = cmd;
    tempCmd += "\n";
    Fw::Buffer commandBuffer(
        reinterpret_cast<U8*>(const_cast<char*>(tempCmd.toChar())),
        tempCmd.length()
    );

    this->commandOut_out(0, commandBuffer, Drv::ByteStreamStatus::OP_OK);
    Fw::LogStringArg logCmd(cmd);
    this->log_ACTIVITY_HI_CommandSuccess(logCmd);
    this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
}

}  // namespace Components

// ======================================================================
// \title  PayloadCom.cpp
// \author robertpendergrast, moisesmata
// \brief  cpp file for PayloadCom component implementation class
// ======================================================================
#include "Os/File.hpp"
#include "Fw/Types/Assert.hpp"
#include "Fw/Types/BasicTypes.hpp"
#include "FprimeZephyrReference/Components/PayloadCom/PayloadCom.hpp"
#include <cstring>
#include <cstdio>

namespace Components {

// ----------------------------------------------------------------------
// Component construction and destruction
// ----------------------------------------------------------------------

PayloadCom ::PayloadCom(const char* const compName)
    : PayloadComComponentBase(compName),
      m_protocolBufferSize(0),
      m_fileOpen(false) {
    // Initialize protocol buffer to zero
    memset(m_protocolBuffer, 0, PROTOCOL_BUFFER_SIZE);
}

PayloadCom ::~PayloadCom() {
    // Close file if still open
    if (m_fileOpen) {
        m_file.close();
        m_fileOpen = false;
    }
}


// ----------------------------------------------------------------------
// Handler implementations for typed input ports
// ----------------------------------------------------------------------


void PayloadCom ::in_port_handler(FwIndexType portNum, Fw::Buffer& buffer, const Drv::ByteStreamStatus& status) {

    this->log_ACTIVITY_LO_UartReceived();

    // Check if we received data successfully
    if (status != Drv::ByteStreamStatus::OP_OK) {
        // Must return buffer even on error to prevent leak
        if (buffer.isValid()) {
            this->bufferReturn_out(0, buffer);
        }
        return;
    }

    this->uartDataOut_out(0, buffer, Drv::ByteStreamStatus::OP_OK);

    sendAck();
        
    // CRITICAL: Return buffer to driver so it can deallocate to BufferManager
    // This matches the ComStub pattern: driver allocates, handler processes, handler returns
    this->bufferReturn_out(0, buffer);
}

// ----------------------------------------------------------------------
// Handler implementations for commands
// ----------------------------------------------------------------------

void PayloadCom ::SEND_COMMAND_cmdHandler(FwOpcodeType opCode, U32 cmdSeq, const Fw::CmdStringArg& cmd) {

    // Append newline to command to send over UART
    Fw::CmdStringArg tempCmd = cmd;  
    tempCmd += "\n";                  
    Fw::Buffer commandBuffer(
        reinterpret_cast<U8*>(const_cast<char*>(tempCmd.toChar())), 
        tempCmd.length()
    );

    // Send command over output port
    Drv::ByteStreamStatus sendStatus = this->out_port_out(0, commandBuffer);

    Fw::LogStringArg logCmd(cmd);
    
    // Log success or failure
    if (sendStatus != Drv::ByteStreamStatus::OP_OK) {
        this->log_WARNING_HI_CommandError(logCmd);
        this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::EXECUTION_ERROR);
        return;
    }
    else {
        this->log_ACTIVITY_HI_CommandSuccess(logCmd);
    }

    this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
}


void PayloadCom ::sendAck(){
    // Send an acknowledgment over UART
    const char* ackMsg = "<MOISES>\n";
    Fw::Buffer ackBuffer(
        reinterpret_cast<U8*>(const_cast<char*>(ackMsg)), 
        strlen(ackMsg)
    );
    this->out_port_out(0, ackBuffer);

}

}  // namespace Components

// ======================================================================
// \title  PayloadCom.cpp
// \author robertpendergrast, moisesmata
// \brief  cpp file for PayloadCom component implementation class
// ======================================================================
#include "PROVESFlightControllerReference/Components/PayloadCom/PayloadCom.hpp"

#include <cstring>

#include "Fw/Types/BasicTypes.hpp"

namespace Components {

// ----------------------------------------------------------------------
// Component construction and destruction
// ----------------------------------------------------------------------

PayloadCom ::PayloadCom(const char* const compName) : PayloadComComponentBase(compName) {}

PayloadCom ::~PayloadCom() {}

// ----------------------------------------------------------------------
// Handler implementations for typed input ports
// ----------------------------------------------------------------------

void PayloadCom ::uartDataIn_handler(FwIndexType portNum, Fw::Buffer& buffer, const Drv::ByteStreamStatus& status) {
    // this->log_ACTIVITY_LO_UartReceived();

    // Check if we received data successfully
    if (status != Drv::ByteStreamStatus::OP_OK) {
        // Must return buffer even on error to prevent leak
        if (buffer.isValid()) {
            this->bufferReturn_out(0, buffer);
        }
        return;
    }

    // Forward data to specific payload handler for protocol processing
    this->uartDataOut_out(0, buffer, status);

    // Send ACK to acknowledge receipt
    sendAck();

    // CRITICAL: Return buffer to driver so it can deallocate to BufferManager
    // This matches the ComStub pattern: driver allocates, handler processes, handler returns
    this->bufferReturn_out(0, buffer);
}

void PayloadCom ::commandIn_handler(FwIndexType portNum, Fw::Buffer& buffer, const Drv::ByteStreamStatus& status) {
    // Log received command for debugging
    if (buffer.isValid()) {
        Fw::LogStringArg logStr("Forwarding command");
        this->log_ACTIVITY_HI_CommandForwardSuccess(logStr);
    }

    // Forward command from CameraHandler to UART
    // uartForward is ByteStreamSend which returns status
    Drv::ByteStreamStatus sendStatus = this->uartForward_out(0, buffer);

    // Log if send failed (optional)
    if (sendStatus != Drv::ByteStreamStatus::OP_OK) {
        Fw::LogStringArg logStr("command");
        this->log_WARNING_HI_CommandForwardError(logStr);
    } else {
        Fw::LogStringArg logStr("command");
        this->log_ACTIVITY_HI_CommandForwardSuccess(logStr);
    }
}

// ----------------------------------------------------------------------
// Helper method implementations
// ----------------------------------------------------------------------

void PayloadCom ::sendAck() {
    // Send an acknowledgment over UART
    const char* ackMsg = "<MOISES>\n";
    Fw::Buffer ackBuffer(reinterpret_cast<U8*>(const_cast<char*>(ackMsg)), strlen(ackMsg));
    // uartForward is ByteStreamSend which returns status
    Drv::ByteStreamStatus sendStatus = this->uartForward_out(0, ackBuffer);

    if (sendStatus == Drv::ByteStreamStatus::OP_OK) {
        // this->log_ACTIVITY_LO_AckSent();
    } else {
        Fw::LogStringArg logStr("ACK");
        this->log_WARNING_HI_CommandForwardError(logStr);
    }
}

}  // namespace Components

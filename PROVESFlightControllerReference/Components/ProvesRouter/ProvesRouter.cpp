// ======================================================================
// \title  ProvesRouter.cpp
// \author Ines (based on thomas-bc's FprimeRouter.cpp)
// \brief  cpp file for ProvesRouter component implementation class
// ======================================================================

#include "PROVESFlightControllerReference/Components/ProvesRouter/ProvesRouter.hpp"

#include <Fw/Log/LogString.hpp>
#include <Fw/Time/Time.hpp>
#include <Fw/Types/String.hpp>

#include "Fw/Com/ComPacket.hpp"
#include "Fw/FPrimeBasicTypes.hpp"
#include "Fw/Logger/Logger.hpp"
#include "config/ApidEnumAc.hpp"

namespace Svc {

// ----------------------------------------------------------------------
// Component construction and destruction
// ----------------------------------------------------------------------

ProvesRouter ::ProvesRouter(const char* const compName) : ProvesRouterComponentBase(compName) {}
ProvesRouter ::~ProvesRouter() {}

// ----------------------------------------------------------------------
// Handler implementations for user-defined typed input ports
// ----------------------------------------------------------------------

void ProvesRouter ::dataIn_handler(FwIndexType portNum, Fw::Buffer& packetBuffer, const ComCfg::FrameContext& context) {
    Fw::SerializeStatus status;
    Fw::ComPacketType packetType = context.get_apid();
    // Route based on received APID (packet type)
    switch (packetType) {
        // Handle a command packet
        case Fw::ComPacketType::FW_PACKET_COMMAND: {
            // Allocate a com buffer on the stack
            Fw::ComBuffer com;
            // Copy the contents of the packet buffer into the com buffer
            status = com.setBuff(packetBuffer.getData(), packetBuffer.getSize());
            if (status == Fw::FW_SERIALIZE_OK) {
                // Send the com buffer - critical functionality so it is considered an error not to
                // have the port connected. This is why we don't check isConnected() before sending.
                this->commandOut_out(0, com, 0);
            } else {
                this->log_WARNING_HI_SerializationError(status);
            }
            break;
        }
        // Handle a file packet
        case Fw::ComPacketType::FW_PACKET_FILE: {
            // If the file uplink output port is connected, send the file packet. Otherwise take no action.
            if (this->isConnected_fileOut_OutputPort(0)) {
                // Copy buffer into a new allocated buffer. This lets us return the original buffer with dataReturnOut,
                // and FprimeRouter can handle the deallocation of the file buffer when it returns on fileBufferReturnIn
                Fw::Buffer packetBufferCopy = this->bufferAllocate_out(0, packetBuffer.getSize());
                // Confirm we got a valid buffer before using it
                if (packetBufferCopy.isValid()) {
                    auto copySerializer = packetBufferCopy.getSerializer();
                    status = copySerializer.serializeFrom(packetBuffer.getData(), packetBuffer.getSize(),
                                                          Fw::Serialization::OMIT_LENGTH);
                    FW_ASSERT(status == Fw::FW_SERIALIZE_OK, status);
                    // Send the copied buffer out. It will come back on fileBufferReturnIn once the receiver is done
                    // with it
                    this->fileOut_out(0, packetBufferCopy);
                } else {
                    this->log_WARNING_HI_AllocationError(ProvesRouter_AllocationReason::FILE_UPLINK);
                }
            }
            break;
        }
        default: {
            // Packet type is not known to the F Prime protocol. If the unknownDataOut port is
            // connected, forward packet and context for further processing
            if (this->isConnected_unknownDataOut_OutputPort(0)) {
                // Copy buffer into a new allocated buffer. This lets us return the original buffer with dataReturnOut,
                // and FprimeRouter can handle the deallocation of the unknown buffer when it returns on bufferReturnIn
                Fw::Buffer packetBufferCopy = this->bufferAllocate_out(0, packetBuffer.getSize());
                // Confirm we got a valid buffer before using it
                if (packetBufferCopy.isValid()) {
                    auto copySerializer = packetBufferCopy.getSerializer();
                    status = copySerializer.serializeFrom(packetBuffer.getData(), packetBuffer.getSize(),
                                                          Fw::Serialization::OMIT_LENGTH);
                    FW_ASSERT(status == Fw::FW_SERIALIZE_OK, status);
                    // Send the copied buffer out. It will come back on fileBufferReturnIn once the receiver is done
                    // with it
                    this->unknownDataOut_out(0, packetBufferCopy, context);
                } else {
                    this->log_WARNING_HI_AllocationError(ProvesRouter_AllocationReason::USER_BUFFER);
                }
            }
            break;
        }
    }

    // Notify interested components that a packet has been authenticated and routed
    if (this->isConnected_packetRouted_OutputPort(0)) {
        this->packetRouted_out(0);
    }

    // Return ownership of the incoming packetBuffer
    this->dataReturnOut_out(0, packetBuffer, context);
}

void ProvesRouter ::cmdResponseIn_handler(FwIndexType portNum,
                                          FwOpcodeType opcode,
                                          U32 cmdSeq,
                                          const Fw::CmdResponse& response) {
    // Nothing to do
}

void ProvesRouter ::fileBufferReturnIn_handler(FwIndexType portNum, Fw::Buffer& fwBuffer) {
    this->bufferDeallocate_out(0, fwBuffer);
}

}  // namespace Svc

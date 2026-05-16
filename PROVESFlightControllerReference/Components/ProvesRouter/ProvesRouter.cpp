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

    // Handle command packets
    if (packetType == Fw::ComPacketType::FW_PACKET_COMMAND) {
        // Allocate a com buffer on the stack
        Fw::ComBuffer com;
        // Copy the contents of the packet buffer into the com buffer
        status = com.setBuff(packetBuffer.getData(), packetBuffer.getSize());
        if (status != Fw::FW_SERIALIZE_OK) {
            // Telemeter the error
            this->log_WARNING_HI_SerializationError(status);

            // Return frame buffer ownership for deallocation
            this->dataReturnOut_out(0, packetBuffer, context);
            return;
        }

        this->commandOut_out(0, com, 0);

        // Notify interested components that a packet has been authenticated and routed
        if (this->isConnected_packetRouted_OutputPort(0)) {
            this->packetRouted_out(0);
        }

        // Return frame buffer ownership for deallocation
        this->dataReturnOut_out(0, packetBuffer, context);
        return;
    }

    // Handle file packets
    if (packetType == Fw::ComPacketType::FW_PACKET_FILE) {
        // Exit early if file uplink output port is not connected
        if (!this->isConnected_fileOut_OutputPort(0)) {
            // Return frame buffer ownership for deallocation
            this->dataReturnOut_out(0, packetBuffer, context);
            return;
        }

        // Copy buffer into a new allocated buffer. This lets us return the original buffer with dataReturnOut,
        // and FprimeRouter can handle the deallocation of the file buffer when it returns on fileBufferReturnIn
        Fw::Buffer packetBufferCopy = this->bufferAllocate_out(0, packetBuffer.getSize());

        // Confirm we got a valid buffer before using it
        if (!packetBufferCopy.isValid()) {
            // Telemeter the error
            this->log_WARNING_HI_AllocationError(ProvesRouter_AllocationReason::FILE_UPLINK);

            // Return frame buffer ownership for deallocation
            this->dataReturnOut_out(0, packetBuffer, context);
        }

        // Copy the contents of the packet buffer into the new buffer
        auto copySerializer = packetBufferCopy.getSerializer();
        status = copySerializer.serializeFrom(packetBuffer.getData(), packetBuffer.getSize(),
                                              Fw::Serialization::OMIT_LENGTH);
        if (status != Fw::FW_SERIALIZE_OK) {
            // Telemeter the error
            this->log_WARNING_HI_SerializationError(status);

            // Return frame buffer ownership for deallocation
            this->dataReturnOut_out(0, packetBuffer, context);
            return;
        }

        // Send the copied buffer out. It will come back on fileBufferReturnIn once the receiver is done
        // with it
        this->fileOut_out(0, packetBufferCopy);

        // Notify interested components that a packet has been authenticated and routed
        if (this->isConnected_packetRouted_OutputPort(0)) {
            this->packetRouted_out(0);
        }

        // Return frame buffer ownership for deallocation
        this->dataReturnOut_out(0, packetBuffer, context);
        return;
    }

    // Handle unknown packet types, if the unknownDataOut port is connected.
    if (!this->isConnected_unknownDataOut_OutputPort(0)) {
        // Copy buffer into a new allocated buffer. This lets us return the original buffer with dataReturnOut,
        // and FprimeRouter can handle the deallocation of the unknown buffer when it returns on bufferReturnIn
        Fw::Buffer packetBufferCopy = this->bufferAllocate_out(0, packetBuffer.getSize());

        // Confirm we got a valid buffer before using it
        if (!packetBufferCopy.isValid()) {
            // Telemeter the error
            this->log_WARNING_HI_AllocationError(ProvesRouter_AllocationReason::USER_BUFFER);

            // Return frame buffer ownership for deallocation
            this->dataReturnOut_out(0, packetBuffer, context);
            return;
        }

        auto copySerializer = packetBufferCopy.getSerializer();
        status = copySerializer.serializeFrom(packetBuffer.getData(), packetBuffer.getSize(),
                                              Fw::Serialization::OMIT_LENGTH);
        if (status != Fw::FW_SERIALIZE_OK) {
            // Telemeter the error
            this->log_WARNING_HI_SerializationError(status);

            // Return frame buffer ownership for deallocation
            this->dataReturnOut_out(0, packetBuffer, context);
            return;
        }

        // Send the copied buffer out. It will come back on fileBufferReturnIn once the receiver is done
        // with it
        this->unknownDataOut_out(0, packetBufferCopy, context);

        // Notify interested components that a packet has been authenticated and routed
        if (this->isConnected_packetRouted_OutputPort(0)) {
            this->packetRouted_out(0);
        }

        // Return frame buffer ownership for deallocation
        this->dataReturnOut_out(0, packetBuffer, context);
        return;
    }
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

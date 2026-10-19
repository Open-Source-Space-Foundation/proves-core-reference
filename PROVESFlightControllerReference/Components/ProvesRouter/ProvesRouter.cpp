// ======================================================================
// \title  ProvesRouter.cpp
// \author Ines (based on thomas-bc's FprimeRouter.cpp)
// \brief  cpp file for ProvesRouter component implementation class
// ======================================================================

#include "PROVESFlightControllerReference/Components/ProvesRouter/ProvesRouter.hpp"

#include <Fw/Log/LogString.hpp>
#include <Fw/Time/Time.hpp>
#include <Fw/Types/String.hpp>

#include "Bypasser.hpp"
#include "Fw/Com/ComPacket.hpp"
#include "Fw/FPrimeBasicTypes.hpp"
#include "Fw/Logger/Logger.hpp"
#include "RoutingPolicy.hpp"
#include "config/ApidEnumAc.hpp"

namespace Svc {

// ----------------------------------------------------------------------
// Component construction and destruction
// ----------------------------------------------------------------------

ProvesRouter ::ProvesRouter(const char* const compName)
    : ProvesRouterComponentBase(compName), m_routedPackets(0), m_bypassedPackets(0), m_rejectedPackets(0) {}
ProvesRouter ::~ProvesRouter() {}

// ----------------------------------------------------------------------
// Handler implementations for user-defined typed input ports
// ----------------------------------------------------------------------

void ProvesRouter ::dataIn_handler(FwIndexType portNum, Fw::Buffer& packetBuffer, const ComCfg::FrameContext& context) {
    // Validate that the packet is authenticated or can bypass authentication
    bool allowed = context.get_authenticated() ||
                   Components::PacketBypasser::bypassPacket(packetBuffer.getData(), packetBuffer.getSize());
    if (!allowed) {
        // Telemeter the rejection
        this->m_rejectedPackets += 1;
        this->tlmWrite_RejectedPackets(this->m_rejectedPackets);

        // Return frame buffer ownership
        this->dataReturnOut_out(0, packetBuffer, context);
        return;
    }

    // Telemeter the bypass if the packet was allowed to bypass authentication
    bool bypassed = !context.get_authenticated() && allowed;
    if (bypassed) {
        this->m_bypassedPackets += 1;
        this->tlmWrite_BypassedPackets(this->m_bypassedPackets);
    }

    // Route based on packet type
    bool routed = false;
    Fw::ComPacketType packetType = context.get_apid();
    switch (packetType) {
        case Fw::ComPacketType::FW_PACKET_COMMAND:
            routed = this->handleCommandPacket(packetBuffer);
            break;
        case Fw::ComPacketType::FW_PACKET_FILE:
            routed = this->handleFilePacket(packetBuffer);
            break;
        default:
            routed = this->handleUnknownPacket(packetBuffer, context);
            break;
    }

    if (routed) {
        this->m_routedPackets += 1;
        this->tlmWrite_RoutedPackets(this->m_routedPackets);
    }

    if (Components::RoutingPolicy::resetsCommandLoss(context.get_authenticated(), routed) &&
        this->isConnected_authenticatedPacketRouted_OutputPort(0)) {
        this->authenticatedPacketRouted_out(0);
    }

    // Return frame buffer ownership
    this->dataReturnOut_out(0, packetBuffer, context);
}

bool ProvesRouter::handleCommandPacket(Fw::Buffer& packetBuffer) {
    // Allocate a com buffer on the stack
    Fw::ComBuffer com;

    // Copy the contents of the packet buffer into the com buffer
    Fw::SerializeStatus status = com.setBuff(packetBuffer.getData(), packetBuffer.getSize());
    if (status != Fw::FW_SERIALIZE_OK) {
        // Telemeter the error
        this->log_WARNING_HI_SerializationError(status);
        return false;
    }

    // Send the com buffer to connected components
    this->commandOut_out(0, com, 0);
    return true;
}

bool ProvesRouter::handleFilePacket(Fw::Buffer& packetBuffer) {
    // Exit early if no components are connected
    if (!this->isConnected_fileOut_OutputPort(0)) {
        return false;
    }

    // Copy buffer into a new allocated buffer. This lets us return the original buffer with dataReturnOut,
    // and ProvesRouter can handle the deallocation of the unknown buffer when it returns on bufferReturnIn
    Fw::Buffer copy = this->allocateCopy(packetBuffer, ProvesRouter_AllocationReason::FILE_UPLINK);
    if (!copy.isValid()) {
        return false;
    }

    // Send the copied buffer to connected components
    this->fileOut_out(0, copy);
    return true;
}

bool ProvesRouter::handleUnknownPacket(Fw::Buffer& packetBuffer, const ComCfg::FrameContext& context) {
    // Exit early if no components are connected
    if (!this->isConnected_unknownDataOut_OutputPort(0)) {
        return false;
    }

    // Copy buffer into a new allocated buffer. This lets us return the original buffer with dataReturnOut,
    // and ProvesRouter can handle the deallocation of the unknown buffer when it returns on bufferReturnIn
    Fw::Buffer copy = this->allocateCopy(packetBuffer, ProvesRouter_AllocationReason::USER_BUFFER);
    if (!copy.isValid()) {
        return false;
    }

    // Send the copied buffer to connected components
    this->unknownDataOut_out(0, copy, context);
    return true;
}

Fw::Buffer ProvesRouter ::allocateCopy(Fw::Buffer& src, ProvesRouter_AllocationReason reason) {
    Fw::Buffer copy = this->bufferAllocate_out(0, src.getSize());
    if (!copy.isValid()) {
        // Telemeter the error
        this->log_WARNING_HI_AllocationError(reason);
        return copy;
    }

    auto serializer = copy.getSerializer();
    Fw::SerializeStatus status = serializer.serializeFrom(src.getData(), src.getSize(), Fw::Serialization::OMIT_LENGTH);
    if (status != Fw::FW_SERIALIZE_OK) {
        // Telemeter the error
        this->log_WARNING_HI_SerializationError(status);
        this->bufferDeallocate_out(0, copy);
        return Fw::Buffer();
    }
    return copy;
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

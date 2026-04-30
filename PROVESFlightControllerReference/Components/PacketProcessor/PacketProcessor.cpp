// ======================================================================
// \title  PacketProcessor.cpp
// \brief  cpp file for PacketProcessor component implementation class
// ======================================================================

#include "PROVESFlightControllerReference/Components/PacketProcessor/PacketProcessor.hpp"

#include <psa/crypto.h>

#include <FprimeExtras/Utilities/FileHelper/FileHelper.hpp>
#include <Fw/Log/LogString.hpp>
#include <iomanip>
#include <utility>

// Include generated header with default key (generated at build time)
#include "AuthDefaultKey.h"
#include "Types.hpp"

namespace Components {

// ----------------------------------------------------------------------
// Component construction and destruction
// ----------------------------------------------------------------------

PacketProcessor ::PacketProcessor(const char* const compName)
    : PacketProcessorComponentBase(compName),
      m_sequenceNumberFilePath(),
      m_sequenceNumber(0),
      m_sequenceNumberWindow(0),
      m_bypassPacketsCount(0),
      m_rejectedPacketsCount(0) {}

PacketProcessor ::~PacketProcessor() {}

// ----------------------------------------------------------------------
// Handler implementations for typed input ports
// ----------------------------------------------------------------------

void PacketProcessor ::dataIn_handler(FwIndexType portNum, Fw::Buffer& data, const ComCfg::FrameContext& context) {
    // Parse the packet to extract relevant information for validation and authentication
    const PacketParser::Result parseResult = parsePacket(data.getData(), data.getSize());

    // If there was an error parsing the packet, reject it
    if (parseResult.status != PacketParser::Status::Ok) {
        this->log_WARNING_HI_ParsingFailed(static_cast<PacketParserStatus::T>(parseResult.status));
        this->rejectPacket(data, context);
        return;
    }

    // Validate the packet to determine if it can bypass authentication, should be rejected, or is eligible for
    // authentication
    PacketValidator::Status validationStatus =
        validatePacket(parseResult.packet, this->m_sequenceNumber, this->m_sequenceNumberWindow);

    // If the packet can bypass authentication
    if (validationStatus == PacketValidator::Status::Bypass) {
        this->bypassPacket(data, context);
        return;
    }

    // If the packet failed validation due to invalid SPI value, reject it
    if (validationStatus == PacketValidator::Status::SpiInvalid) {
        this->log_WARNING_HI_SpiInvalid(parseResult.packet.spi);
        this->rejectPacket(data, context);
        return;
    }

    // If the packet failed validation due to sequence number being out of the acceptable window, reject it
    if (validationStatus == PacketValidator::Status::SequenceNumberOutOfWindow) {
        this->log_WARNING_HI_SequenceNumberOutOfWindow(this->m_sequenceNumber, this->m_sequenceNumberWindow);
        this->rejectPacket(data, context);
        return;
    }

    // Authenticate the packet
    const PacketAuthenticator::Result authResult =
        authenticatePacket(data.getData(), data.getSize(), parseResult.packet.hmac);

    // If the packet failed authentication, reject it
    if (authResult.status != PacketAuthenticator::Status::Authenticated) {
        this->log_WARNING_HI_AuthenticationFailed(static_cast<PacketAuthenticatorStatus::T>(authResult.status),
                                                  authResult.psaStatus);
        this->rejectPacket(data, context);
        return;
    }

    // If the packet was successfully authenticated, accept it
    this->acceptPacket(data, context);
}

void PacketProcessor ::dataReturnIn_handler(FwIndexType portNum,
                                            Fw::Buffer& data,
                                            const ComCfg::FrameContext& context) {
    this->dataReturnOut_out(0, data, context);
}

// ----------------------------------------------------------------------
// Handler implementations for commands
// ----------------------------------------------------------------------

void PacketProcessor ::GET_SEQ_NUM_cmdHandler(FwOpcodeType opCode, U32 cmdSeq) {
    // Read the sequence number from the file system
    U32 sequenceNumber = 0;
    Os::File::Status status = this->readSequenceNumber(sequenceNumber);
    if (status != Os::File::OP_OK) {
        // Return execution error response
        this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::EXECUTION_ERROR);
        return;
    }

    // Log the successful sequence number get
    this->log_ACTIVITY_HI_SequenceNumberGet(sequenceNumber);

    // Return success response
    this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
}

void PacketProcessor ::SET_SEQ_NUM_cmdHandler(FwOpcodeType opCode, U32 cmdSeq, U32 seq_num) {
    // Write the sequence number to the file system
    Os::File::Status status = this->writeSequenceNumber(seq_num);
    if (status != Os::File::OP_OK) {
        // Return execution error response
        this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::EXECUTION_ERROR);
        return;
    }

    // Log the successful sequence number set
    this->log_ACTIVITY_HI_SequenceNumberSet(seq_num);

    // Return success response
    this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
}

// ----------------------------------------------------------------------
// Public helper methods
// ----------------------------------------------------------------------

void PacketProcessor ::configure() {
    Fw::ParamValid is_valid;

    // Get the file path from the parameter
    this->m_sequenceNumberFilePath = this->paramGet_SEQ_NUM_FILE_PATH(is_valid);
    FW_ASSERT(is_valid == Fw::ParamValid::VALID || is_valid == Fw::ParamValid::DEFAULT);

    // Get the sequence number from the file system
    U32 sequenceNumber;
    Os::File::Status status = this->readSequenceNumber(sequenceNumber);
    FW_ASSERT(status == Os::File::OP_OK);

    // Telemeter the current sequence number
    this->tlmWrite_CurrentSequenceNumber(sequenceNumber);

    // Get the sequence number window size from the parameter
    this->m_sequenceNumberWindow = this->paramGet_SEQ_NUM_WINDOW(is_valid);
    FW_ASSERT(is_valid == Fw::ParamValid::VALID || is_valid == Fw::ParamValid::DEFAULT);
}

// ----------------------------------------------------------------------
// Private helper methods
// ----------------------------------------------------------------------

Os::File::Status PacketProcessor ::readSequenceNumber(U32& value) {
    // Read the sequence number from the file system
    Os::File::Status status = Utilities::FileHelper::readFromFile(this->m_sequenceNumberFilePath.toChar(), value);
    if (status != Os::File::OP_OK) {
        // Log the failure to read the sequence number
        this->log_WARNING_HI_SequenceNumberReadFailed(static_cast<Os::FileStatus::T>(status));
    }

    // If the sequence number file does not exist, write it to disk with the default value of 0
    if (status != Os::File::DOESNT_EXIST) {
        return this->writeSequenceNumber(0);
    }

    return status;
}

Os::File::Status PacketProcessor ::writeSequenceNumber(const U32 value) {
    Os::File::Status status = Utilities::FileHelper::writeToFile(this->m_sequenceNumberFilePath.toChar(), value);
    if (status != Os::File::OP_OK) {
        // Log the failure to write the default sequence number
        this->log_WARNING_HI_SequenceNumberWriteFailed(static_cast<Os::FileStatus::T>(status));
    }

    return status;
}

void PacketProcessor ::acceptPacket(Fw::Buffer& data, const ComCfg::FrameContext& context) {
    // Update the sequence number and write it to disk
    this->m_sequenceNumber += 1;
    this->writeSequenceNumber(this->m_sequenceNumber);

    // Write sequence number to persistent storage
    // intentionally not checking the return value
    this->writeSequenceNumber(this->m_sequenceNumber);

    // Authenticate the packet
    ComCfg::FrameContext contextOut = context;
    contextOut.set_authenticated(true);

    // Forward the packet to the output port
    this->forwardPacket(data, contextOut);
}

void PacketProcessor ::bypassPacket(Fw::Buffer& data, const ComCfg::FrameContext& context) {
    // Telemeter the updated bypass packets count
    this->m_bypassPacketsCount += 1;
    this->tlmWrite_BypassPacketsCount(this->m_bypassPacketsCount);

    // Forward the packet to the output port
    this->forwardPacket(data, context);
}

void PacketProcessor ::forwardPacket(Fw::Buffer& data, const ComCfg::FrameContext& context) {
    // Strip header and trailer from the buffer for forwarding
    data.setData(data.getData() + Ccsds355_0_B_2_Cmac::kSecurityHeaderSize);
    data.setSize(data.getSize() - Ccsds355_0_B_2_Cmac::kSecurityHeaderSize - Ccsds355_0_B_2_Cmac::kSecurityTrailerSize);

    // Forward the packet to the output port
    this->dataOut_out(0, data, context);
}

void PacketProcessor ::rejectPacket(Fw::Buffer& data, const ComCfg::FrameContext& context) {
    // Telemeter the updated rejected packet count
    this->m_rejectedPacketsCount += 1;
    this->tlmWrite_RejectedPacketsCount(this->m_rejectedPacketsCount);

    // Do not forward the packet to the output port
    this->dataReturnOut_out(0, data, context);
}

}  // namespace Components

// ======================================================================
// \title  PacketProcessor.cpp
// \brief  cpp file for PacketProcessor component implementation class
// ======================================================================

#include "PROVESFlightControllerReference/Components/PacketProcessor/PacketProcessor.hpp"

#include <FprimeExtras/Utilities/FileHelper/FileHelper.hpp>
#include <Fw/Log/LogString.hpp>
#include <iomanip>
#include <utility>

#include "Authenticator.hpp"
#include "PacketProcessor.hpp"
#include "Types.hpp"

// Include generated header with default key (generated at build time)
#include "AuthDefaultKey.h"

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
    this->log_WARNING_HI_ParsingFailed_ThrottleClear();

    // For easier readability of subsequent code, create a reference to the parsed packet
    const Components::Packet& packet = parseResult.packet;

    {
        // Lock sequence number state
        Os::ScopeLock lock(this->m_sequenceNumberLock);

        // Validate the packet to determine if it can bypass authentication, should be rejected, or is eligible for
        // authentication
        PacketValidator::Status validationStatus =
            validatePacket(packet, this->m_sequenceNumber, this->m_sequenceNumberWindow);

        // If the packet can bypass authentication
        if (validationStatus == PacketValidator::Status::Bypass) {
            this->bypassPacket(data, context);
            return;
        }

        // If the packet failed validation due to invalid SPI value, reject it
        if (validationStatus == PacketValidator::Status::SpiInvalid) {
            this->log_WARNING_HI_SpiInvalid(packet.spi);
            this->rejectPacket(data, context);
            return;
        }
        this->log_WARNING_HI_SpiInvalid_ThrottleClear();

        // If the packet failed validation due to sequence number being out of the acceptable window, reject it
        if (validationStatus == PacketValidator::Status::SequenceNumberInvalid) {
            this->log_WARNING_HI_SequenceNumberInvalid(packet.sequenceNumber, this->m_sequenceNumber,
                                                       this->m_sequenceNumberWindow);
            this->rejectPacket(data, context);
            return;
        }
        this->log_WARNING_HI_SequenceNumberInvalid_ThrottleClear();

        // Authenticate the packet
        const PacketAuthenticator::AuthenticationResult authResult =
            authenticatePacket(data.getData(), data.getSize(), packet.hmac, this->m_hmacKeyId);

        // If the packet failed authentication, reject it
        if (authResult.status != PacketAuthenticator::AuthenticationStatus::Authenticated) {
            this->log_WARNING_HI_AuthenticationFailed(static_cast<PacketAuthenticatorStatus::T>(authResult.status),
                                                      authResult.psaStatus);
            this->rejectPacket(data, context);
            return;
        }
        this->log_WARNING_HI_AuthenticationFailed_ThrottleClear();

        // If the packet was successfully authenticated, accept it (caller holds the mutex)
        this->acceptPacket(data, context, packet.sequenceNumber);
    }
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
    Os::ScopeLock lock(this->m_sequenceNumberLock);

    // Log the successful sequence number get
    this->log_ACTIVITY_HI_SequenceNumberGet(this->m_sequenceNumber);

    // Return success response
    this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
}

void PacketProcessor ::SET_SEQ_NUM_cmdHandler(FwOpcodeType opCode, U32 cmdSeq, U32 seq_num) {
    Os::ScopeLock lock(this->m_sequenceNumberLock);

    // Write the sequence number to the file system
    Os::File::Status status = this->writeSequenceNumber(seq_num);
    if (status != Os::File::OP_OK) {
        // Return execution error response
        this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::EXECUTION_ERROR);
        return;
    }

    // Set runtime sequence number to the new value
    this->m_sequenceNumber = seq_num;

    // Telemeter the updated sequence number
    this->tlmWrite_CurrentSequenceNumber(this->m_sequenceNumber);

    // Log the successful sequence number set
    this->log_ACTIVITY_HI_SequenceNumberSet(this->m_sequenceNumber);

    // Return success response
    this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
}

// ----------------------------------------------------------------------
// Public helper methods
// ----------------------------------------------------------------------

void PacketProcessor ::configure() {
    Os::ScopeLock lock(this->m_sequenceNumberLock);
    Fw::ParamValid is_valid;

    // Get the sequence number window size from the parameter
    this->m_sequenceNumberWindow = this->paramGet_SEQ_NUM_WINDOW(is_valid);
    FW_ASSERT(is_valid == Fw::ParamValid::VALID || is_valid == Fw::ParamValid::DEFAULT);

    // Get the file path from the parameter
    this->m_sequenceNumberFilePath = this->paramGet_SEQ_NUM_FILE_PATH(is_valid);
    FW_ASSERT(is_valid == Fw::ParamValid::VALID || is_valid == Fw::ParamValid::DEFAULT);

    // Get the sequence number from the file system
    U32 sequenceNumber = 0;
    Os::File::Status status = this->readSequenceNumber(sequenceNumber);
    FW_ASSERT(status == Os::File::OP_OK);
    this->m_sequenceNumber = sequenceNumber;

    // Telemeter the current sequence number
    this->tlmWrite_CurrentSequenceNumber(this->m_sequenceNumber);

    // Import the HMAC key
    PacketAuthenticator::KeyImportResult result = importHmacKey(AUTH_DEFAULT_KEY, this->m_hmacKeyId);
    FW_ASSERT(result.status == PacketAuthenticator::KeyImportStatus::Success);
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
    } else {
        // Clear throttle for sequence number read failure
        this->log_WARNING_HI_SequenceNumberReadFailed_ThrottleClear();
    }

    // If the sequence number file does not exist, write it to disk with the default value of 0
    if (status == Os::File::DOESNT_EXIST) {
        return this->writeSequenceNumber(0);
    }

    return status;
}

Os::File::Status PacketProcessor ::writeSequenceNumber(const U32 value) {
    Os::File::Status status = Utilities::FileHelper::writeToFile(this->m_sequenceNumberFilePath.toChar(), value);
    if (status != Os::File::OP_OK) {
        // Log the failure to write the default sequence number
        this->log_WARNING_HI_SequenceNumberWriteFailed(static_cast<Os::FileStatus::T>(status));
    } else {
        // Clear throttle for sequence number write failure
        this->log_WARNING_HI_SequenceNumberWriteFailed_ThrottleClear();
    }

    return status;
}

void PacketProcessor ::acceptPacket(Fw::Buffer& data, const ComCfg::FrameContext& context, const U32 sequenceNumber) {
    // Update the sequence number and write it to disk
    // intentionally not checking the return value
    this->m_sequenceNumber = sequenceNumber;
    this->writeSequenceNumber(this->m_sequenceNumber);
    this->tlmWrite_CurrentSequenceNumber(this->m_sequenceNumber);

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

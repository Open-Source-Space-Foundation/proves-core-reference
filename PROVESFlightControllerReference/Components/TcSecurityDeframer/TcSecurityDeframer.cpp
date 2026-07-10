// ======================================================================
// \title  TcSecurityDeframer.cpp
// \brief  cpp file for TcSecurityDeframer component implementation class
// ======================================================================

#include "PROVESFlightControllerReference/Components/TcSecurityDeframer/TcSecurityDeframer.hpp"

#include <FprimeExtras/Utilities/FileHelper/FileHelper.hpp>
#include <Fw/Log/LogString.hpp>
#include <utility>

#include "Authenticator.hpp"
#include "TcSecurityDeframer.hpp"
#include "Types.hpp"

// Include generated header with default key (generated at build time)
#include "AuthDefaultKey.h"

namespace Components {

// ----------------------------------------------------------------------
// Component construction and destruction
// ----------------------------------------------------------------------

TcSecurityDeframer ::TcSecurityDeframer(const char* const compName)
    : TcSecurityDeframerComponentBase(compName),
      m_sequenceNumberFilePath(),
      m_sequenceNumber(0),
      m_sequenceNumberWindow(0) {}

TcSecurityDeframer ::~TcSecurityDeframer() {}

// ----------------------------------------------------------------------
// Handler implementations for typed input ports
// ----------------------------------------------------------------------

Ccsds::ProcessSecurityResult TcSecurityDeframer ::processSecurity_handler(FwIndexType portNum,
                                                                          U16 globalVcId,
                                                                          U16 globalMapId,
                                                                          Fw::Buffer& payload) {
    // The payload is a full TC Transfer Frame minus the 2-byte FECF:
    //   [TC Primary Header (5)] [Security Header: SPI(2)+SeqNum(4)] [Data Field] [Security Trailer: MAC(16)]
    // Skip the TC Primary Header to reach the Security Header for parsing and authentication.
    const uint8_t* const secData = payload.getData() + Ccsds355_0_B_2::kTCPrimaryHeaderSize;
    const size_t secDataSize = payload.getSize() - Ccsds355_0_B_2::kTCPrimaryHeaderSize;

    // --- Parse Security Header and Trailer ---
    const Ccsds355_0_B_2::TcTransferFrame::Parser::Result parseResult = Ccsds355_0_B_2::parse(secData, secDataSize);
    if (parseResult.status != Ccsds355_0_B_2::TcTransferFrame::Parser::Status::Ok) {
        this->log_WARNING_HI_ParsingFailed(static_cast<PacketParserStatus::T>(parseResult.status));
        Ccsds::ProcessSecurityResult result;
        result.set_status(Ccsds::VerificationStatus::FAILURE);
        result.set_statusCode(static_cast<U8>(Ccsds355_0_B_2::Verification::StatusCode::SpiInvalid));
        return result;
    }
    this->log_WARNING_HI_ParsingFailed_ThrottleClear();

    {
        Os::ScopeLock lock(this->m_sequenceNumberLock);

        // --- Validate SPI and anti-replay sequence number ---
        const PacketValidator::Status validationStatus =
            validatePacket(parseResult.securityHeader, this->m_sequenceNumber, this->m_sequenceNumberWindow);

        if (validationStatus == PacketValidator::Status::SpiInvalid) {
            this->log_WARNING_HI_SpiInvalid(parseResult.securityHeader.spi);
            Ccsds::ProcessSecurityResult result;
            result.set_status(Ccsds::VerificationStatus::FAILURE);
            result.set_statusCode(static_cast<U8>(Ccsds355_0_B_2::Verification::StatusCode::SpiInvalid));
            return result;
        }
        this->log_WARNING_HI_SpiInvalid_ThrottleClear();

        if (validationStatus == PacketValidator::Status::SequenceNumberInvalid) {
            this->log_WARNING_HI_SequenceNumberInvalid(parseResult.securityHeader.sequenceNumber,
                                                       this->m_sequenceNumber, this->m_sequenceNumberWindow);
            Ccsds::ProcessSecurityResult result;
            result.set_status(Ccsds::VerificationStatus::FAILURE);
            result.set_statusCode(static_cast<U8>(Ccsds355_0_B_2::Verification::StatusCode::SequenceNumberFailed));
            return result;
        }
        this->log_WARNING_HI_SequenceNumberInvalid_ThrottleClear();

        // --- Authenticate: HMAC over Security Header + Data Field (same byte range as before port refactor) ---
        const PacketAuthenticator::AuthenticationResult authResult =
            authenticatePacket(secData, secDataSize, parseResult.securityTrailer.mac, this->m_hmacKeyId);

        if (authResult.status != PacketAuthenticator::AuthenticationStatus::Authenticated) {
            this->log_WARNING_HI_AuthenticationFailed(static_cast<PacketAuthenticatorStatus::T>(authResult.status),
                                                      authResult.psaStatus);
            Ccsds::ProcessSecurityResult result;
            result.set_status(Ccsds::VerificationStatus::FAILURE);
            result.set_statusCode(static_cast<U8>(Ccsds355_0_B_2::Verification::StatusCode::MacFailed));
            return result;
        }
        this->log_WARNING_HI_AuthenticationFailed_ThrottleClear();

        // --- Accept: persist new sequence number ---
        this->m_sequenceNumber = parseResult.securityHeader.sequenceNumber;
        this->writeSequenceNumber(this->m_sequenceNumber);
        this->tlmWrite_CurrentSequenceNumber(this->m_sequenceNumber);
    }

    // Adjust payload to expose only the Data Field per CCSDS 355.0-B-2 §3.3.3.3:
    //   start = first octet after Security Header
    //   end   = last octet of the Transfer Frame Data Field (excluding Security Trailer)
    const size_t dataFieldOffset = Ccsds355_0_B_2::kTCPrimaryHeaderSize + Ccsds355_0_B_2::kTCSecurityHeaderSize;
    payload.setData(payload.getData() + dataFieldOffset);
    payload.setSize(payload.getSize() - dataFieldOffset - Ccsds355_0_B_2::kTCSecurityTrailer);

    Ccsds::ProcessSecurityResult result;
    result.set_status(Ccsds::VerificationStatus::NO_FAILURE);
    result.set_statusCode(static_cast<U8>(Ccsds355_0_B_2::Verification::StatusCode::Success));
    return result;
}

// ----------------------------------------------------------------------
// Handler implementations for commands
// ----------------------------------------------------------------------

void TcSecurityDeframer ::GET_SEQ_NUM_cmdHandler(FwOpcodeType opCode, U32 cmdSeq) {
    Os::ScopeLock lock(this->m_sequenceNumberLock);

    // Log the successful sequence number get
    this->log_ACTIVITY_HI_SequenceNumberGet(this->m_sequenceNumber);

    // Return success response
    this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
}

void TcSecurityDeframer ::SET_SEQ_NUM_cmdHandler(FwOpcodeType opCode, U32 cmdSeq, U32 seq_num) {
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

void TcSecurityDeframer ::configure() {
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

Os::File::Status TcSecurityDeframer ::readSequenceNumber(U32& value) {
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

Os::File::Status TcSecurityDeframer ::writeSequenceNumber(const U32 value) {
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

}  // namespace Components

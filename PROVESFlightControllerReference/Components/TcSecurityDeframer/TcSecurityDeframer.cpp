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

void TcSecurityDeframer ::dataIn_handler(FwIndexType portNum, Fw::Buffer& data, const ComCfg::FrameContext& context) {
    ComCfg::FrameContext contextOut = context;
    contextOut.set_authenticated(false);

    // TcDeframer has already stripped the TC Primary Header and FECF, so the buffer is:
    //   [Security Header: SPI(2)+SeqNum(4)] [Data Field] [Security Trailer: MAC(16)]

    // --- Parse Security Header and Trailer ---
    const Ccsds355_0_B_2::TcTransferFrame::Parser::Result parseResult =
        Ccsds355_0_B_2::parse(data.getData(), data.getSize());
    if (parseResult.status != Ccsds355_0_B_2::TcTransferFrame::Parser::Status::Ok) {
        // The frame is too short to contain the security fields, so it cannot be stripped
        // for downstream deframing. Return buffer ownership upstream and drop the frame.
        this->log_WARNING_HI_ParsingFailed(static_cast<PacketParserStatus::T>(parseResult.status));
        this->dataReturnOut_out(0, data, contextOut);
        return;
    }
    this->log_WARNING_HI_ParsingFailed_ThrottleClear();

    {
        Os::ScopeLock lock(this->m_sequenceNumberLock);

        // --- Validate SPI and anti-replay sequence number ---
        const PacketValidator::Status validationStatus =
            validatePacket(parseResult.securityHeader, this->m_sequenceNumber, this->m_sequenceNumberWindow);

        if (validationStatus == PacketValidator::Status::SpiInvalid) {
            this->log_WARNING_HI_SpiInvalid(parseResult.securityHeader.spi);
        } else if (validationStatus == PacketValidator::Status::SequenceNumberInvalid) {
            this->log_WARNING_HI_SequenceNumberInvalid(parseResult.securityHeader.sequenceNumber,
                                                       this->m_sequenceNumber, this->m_sequenceNumberWindow);
        } else {
            this->log_WARNING_HI_SpiInvalid_ThrottleClear();
            this->log_WARNING_HI_SequenceNumberInvalid_ThrottleClear();

            // --- Authenticate: HMAC over Security Header + Data Field ---
            const PacketAuthenticator::AuthenticationResult authResult =
                authenticatePacket(data.getData(), data.getSize(), parseResult.securityTrailer.mac, this->m_hmacKeyId);

            if (authResult.status != PacketAuthenticator::AuthenticationStatus::Authenticated) {
                this->log_WARNING_HI_AuthenticationFailed(static_cast<PacketAuthenticatorStatus::T>(authResult.status),
                                                          authResult.psaStatus);
            } else {
                this->log_WARNING_HI_AuthenticationFailed_ThrottleClear();

                // --- Accept: persist new sequence number ---
                // Only fully verified frames advance the counter, so bypass and replayed
                // frames can never desync ground and spacecraft (issue #426)
                this->m_sequenceNumber = parseResult.securityHeader.sequenceNumber;
                this->writeSequenceNumber(this->m_sequenceNumber);
                this->tlmWrite_CurrentSequenceNumber(this->m_sequenceNumber);
                contextOut.set_authenticated(true);
            }
        }
    }

    // Forward only the Data Field per CCSDS 355.0-B-2 §3.3.3.3:
    //   start = first octet after Security Header
    //   end   = last octet of the Transfer Frame Data Field (excluding Security Trailer)
    // Unverified frames are forwarded with authenticated=false; the router enforces
    // the reject-or-bypass policy.
    data.setData(data.getData() + Ccsds355_0_B_2::kTCSecurityHeaderSize);
    data.setSize(data.getSize() - Ccsds355_0_B_2::kTCSecurityHeaderSize - Ccsds355_0_B_2::kTCSecurityTrailer);

    this->dataOut_out(0, data, contextOut);
}

void TcSecurityDeframer ::dataReturnIn_handler(FwIndexType portNum,
                                               Fw::Buffer& data,
                                               const ComCfg::FrameContext& context) {
    this->dataReturnOut_out(0, data, context);
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

    // Get the sequence number from the file system. On a read failure (already evented
    // by readSequenceNumber) fall back to 0 rather than refusing to boot; the operator
    // can correct the counter with SET_SEQ_NUM.
    U32 sequenceNumber = 0;
    (void)this->readSequenceNumber(sequenceNumber);
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

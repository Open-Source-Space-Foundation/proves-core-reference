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
      m_sequenceNumberWindow(0),
      m_persistedSequenceNumber(0),
      m_hmacKeyId(0) {}

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
        const U32 lastAccepted = this->m_sequenceNumber.load();

        // --- Validate SPI and anti-replay sequence number ---
        const PacketValidator::Status validationStatus =
            validatePacket(parseResult.securityHeader, lastAccepted, this->m_sequenceNumberWindow);

        if (validationStatus == PacketValidator::Status::SpiInvalid) {
            this->log_WARNING_HI_SpiInvalid(parseResult.securityHeader.spi);
        } else if (validationStatus == PacketValidator::Status::SequenceNumberInvalid) {
            this->log_WARNING_HI_SequenceNumberInvalid(parseResult.securityHeader.sequenceNumber, lastAccepted,
                                                       this->m_sequenceNumberWindow);
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

                // --- Accept: advance the sequence number ---
                // Only fully verified frames advance the counter (issue #426). No filesystem I/O
                // here: run_handler persists the counter (see SDD, Sequence Number Persistence).
                const U32 accepted = parseResult.securityHeader.sequenceNumber;
                this->m_sequenceNumber.store(accepted);
                this->tlmWrite_CurrentSequenceNumber(accepted);
                contextOut.set_authenticated(true);
            }
        }
    }

    // Forward only the Data Field per CCSDS 355.0-B-2 §3.3.3.3:
    //   start = first octet after Security Header
    //   end   = last octet of the Transfer Frame Data Field (excluding Security Trailer)
    // Unverified frames are forwarded with authenticated=false; the router enforces
    // the reject-or-bypass policy.
    data.advance(Ccsds355_0_B_2::kTCSecurityHeaderSize);
    data.setSize(data.getSize() - Ccsds355_0_B_2::kTCSecurityTrailer);

    this->dataOut_out(0, data, contextOut);
}

void TcSecurityDeframer ::dataReturnIn_handler(FwIndexType portNum,
                                               Fw::Buffer& data,
                                               const ComCfg::FrameContext& context) {
    // Restore the original buffer pointer and size stripped in dataIn_handler so the
    // upstream BufferManager deallocates the exact allocation it originally handed out.
    data.advance(-static_cast<FwSignedSizeType>(Ccsds355_0_B_2::kTCSecurityHeaderSize));
    data.setSize(data.getSize() + Ccsds355_0_B_2::kTCSecurityTrailer);

    this->dataReturnOut_out(0, data, context);
}

void TcSecurityDeframer ::run_handler(FwIndexType portNum, U32 context) {
    this->persistIfChanged();
}

void TcSecurityDeframer ::prepareForReboot_handler(FwIndexType portNum) {
    // The frame that commanded this reboot advanced the counter microseconds ago; without this flush it
    // would still be valid after the reboot. The write is serialized against a tick write in progress.
    this->persistIfChanged();
}

// ----------------------------------------------------------------------
// Handler implementations for commands
// ----------------------------------------------------------------------

void TcSecurityDeframer ::GET_SEQ_NUM_cmdHandler(FwOpcodeType opCode, U32 cmdSeq) {
    Os::ScopeLock lock(this->m_sequenceNumberLock);

    // Log the successful sequence number get
    this->log_ACTIVITY_HI_SequenceNumberGet(this->m_sequenceNumber.load());

    // Return success response
    this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
}

void TcSecurityDeframer ::SET_SEQ_NUM_cmdHandler(FwOpcodeType opCode, U32 cmdSeq, U32 seq_num) {
    Os::ScopeLock lock(this->m_sequenceNumberLock);
    Os::ScopeLock persistLock(this->m_persistLock);

    // Write the sequence number to the file system immediately
    Os::File::Status status = this->writeSequenceNumber(seq_num);
    if (status != Os::File::OP_OK) {
        // Return execution error response
        this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::EXECUTION_ERROR);
        return;
    }

    // Set runtime sequence number to the new value and record that it is on disk
    this->m_sequenceNumber.store(seq_num);
    this->m_persistedSequenceNumber = seq_num;

    // Telemeter the updated sequence number
    this->tlmWrite_CurrentSequenceNumber(seq_num);

    // Log the successful sequence number set
    this->log_ACTIVITY_HI_SequenceNumberSet(seq_num);

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
    const Os::File::Status status = this->readSequenceNumber(sequenceNumber);
    this->m_sequenceNumber.store(sequenceNumber);

    // The restored value is the on-disk baseline for run_handler: the file is rewritten only once the
    // counter moves on, never preemptively (an unreadable file is left as-is until then).
    static_cast<void>(status);
    this->m_persistedSequenceNumber = sequenceNumber;

    // Telemeter the current sequence number
    this->tlmWrite_CurrentSequenceNumber(sequenceNumber);

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

void TcSecurityDeframer ::persistIfChanged() {
    // Only m_persistLock is taken here, never m_sequenceNumberLock: the frame-processing thread must not
    // wait on filesystem I/O. The counter is read under m_persistLock so a concurrent SET_SEQ_NUM (which
    // updates counter and bookkeeping together) cannot be undone by a stale snapshot, and the two
    // callers (run tick, prepareForReboot) cannot write the file concurrently.
    Os::ScopeLock lock(this->m_persistLock);
    const U32 current = this->m_sequenceNumber.load();
    if (!SequencePersistence::needsWrite(this->m_persistedSequenceNumber, current)) {
        return;
    }

    // A failure is evented by writeSequenceNumber; the bookkeeping is left unchanged so the next
    // call retries while the in-memory counter keeps advancing.
    const bool written = (this->writeSequenceNumber(current) == Os::File::OP_OK);
    this->m_persistedSequenceNumber =
        SequencePersistence::afterWrite(this->m_persistedSequenceNumber, current, written);
}

}  // namespace Components

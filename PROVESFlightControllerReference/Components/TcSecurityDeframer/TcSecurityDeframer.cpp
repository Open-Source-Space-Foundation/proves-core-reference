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
      m_persistedHighWater(0),
      m_sequenceNumberArmed(false) {}

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

        // If the persisted sequence-number record failed torn-write validation on boot, the
        // component is UNARMED: reject every frame (valid or not) until ground re-arms it via
        // SET_SEQ_NUM. This is the conservative recovery path for issue #461's persistence fix --
        // see SequenceNumberRecordInvalid and m_sequenceNumberArmed.
        if (!this->m_sequenceNumberArmed) {
            this->log_WARNING_HI_SequenceNumberInvalid(parseResult.securityHeader.sequenceNumber,
                                                       this->m_sequenceNumber, this->m_sequenceNumberWindow);
            this->dataReturnOut_out(0, data, contextOut);
            return;
        }

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

                // --- Accept: advance the in-RAM sequence number (authoritative for runtime
                // acceptance decisions) and persist a write-ahead high-water mark only every
                // SEQ_NUM_PERSIST_STRIDE frames (issue #461: the previous per-command persist here
                // raced FileUplink/FileManager/FileDownlink/PrmDb's own filesystem access on the
                // shared SD-card-backed FatFs mount).
                // Only fully verified frames advance the counter, so bypass and replayed
                // frames can never desync ground and spacecraft (issue #426)
                this->m_sequenceNumber = parseResult.securityHeader.sequenceNumber;
                this->writeAheadPersistIfNeeded(this->m_sequenceNumber);
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
    // Restore the original buffer pointer and size stripped in dataIn_handler so the
    // upstream BufferManager deallocates the exact allocation it originally handed out.
    data.setData(data.getData() - Ccsds355_0_B_2::kTCSecurityHeaderSize);
    data.setSize(data.getSize() + Ccsds355_0_B_2::kTCSecurityHeaderSize + Ccsds355_0_B_2::kTCSecurityTrailer);

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

    // Explicit ground command: persist immediately (not subject to the write-ahead stride --
    // an operator-issued SET_SEQ_NUM is inherently infrequent and is the one path that MUST take
    // effect durably right away, including re-arming after a SequenceNumberRecordInvalid).
    Os::File::Status status = this->writeSequenceNumber(seq_num);
    if (status != Os::File::OP_OK) {
        // Return execution error response
        this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::EXECUTION_ERROR);
        return;
    }

    // Set runtime sequence number to the new value and track the persisted high-water mark
    this->m_sequenceNumber = seq_num;
    this->m_persistedHighWater = seq_num;

    // A ground-issued SET_SEQ_NUM is the documented recovery path after
    // SequenceNumberRecordInvalid -- re-arm the component now that an operator has confirmed a
    // trustworthy sequence number.
    this->m_sequenceNumberArmed = true;

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

    // Get the persisted high-water mark from the file system. readSequenceNumber() arms the
    // component on a genuine first boot (DOESNT_EXIST -> bootstraps to 0) or a successfully
    // validated record; it leaves the component UNARMED (rejecting all frames) on any other
    // failure, including a torn-write checksum mismatch -- see SequenceNumberRecordInvalid.
    // The window still starts at this value either way (unchanged semantics from before this fix).
    U32 sequenceNumber = 0;
    (void)this->readSequenceNumber(sequenceNumber);
    this->m_sequenceNumber = sequenceNumber;
    this->m_persistedHighWater = sequenceNumber;

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
    // Persisted record layout: a single U64 = (value:32 << 32) | (~value:32). This is a minimal
    // torn-write guard -- if power is lost mid-write, FatFs/the SD card may leave a partially
    // written U64 whose two halves don't correspond, which the checksum catches. (See issue #461
    // for how this record is now written -- write-ahead, batched -- rather than on every command.)
    U64 record = 0;
    Os::File::Status status = Utilities::FileHelper::readFromFile(this->m_sequenceNumberFilePath.toChar(), record);

    if (status == Os::File::DOESNT_EXIST) {
        // Genuine first boot: no risk of replay since nothing has ever been accepted. Bootstrap
        // to 0 and arm normally -- this mirrors the pre-fix behavior for this specific case.
        this->log_WARNING_HI_SequenceNumberReadFailed_ThrottleClear();
        value = 0;
        this->m_sequenceNumberArmed = true;
        return this->writeSequenceNumber(0);
    }

    if (status != Os::File::OP_OK) {
        // Genuine I/O failure (not a missing file, not (yet) a checksum question). Treat the same
        // as a torn/invalid record: do not guess a value, stay unarmed until SET_SEQ_NUM.
        this->log_WARNING_HI_SequenceNumberReadFailed(static_cast<Os::FileStatus::T>(status));
        this->log_WARNING_HI_SequenceNumberRecordInvalid(0);
        value = 0;
        this->m_sequenceNumberArmed = false;
        return status;
    }
    this->log_WARNING_HI_SequenceNumberReadFailed_ThrottleClear();

    const U32 storedValue = static_cast<U32>(record >> 32);
    const U32 storedChecksum = static_cast<U32>(record & 0xFFFFFFFFu);
    if (storedChecksum != static_cast<U32>(~storedValue)) {
        // Checksum mismatch: torn write or corruption. Do NOT default to 0/low -- that would
        // reopen the anti-replay window below whatever the real high-water mark was. Instead,
        // leave the component UNARMED (rejects all frames) until ground re-arms it via
        // SET_SEQ_NUM, per the component's existing recovery procedure for sequence-number issues.
        this->log_WARNING_HI_SequenceNumberRecordInvalid(storedValue);
        value = 0;
        this->m_sequenceNumberArmed = false;
        return Os::File::Status::OTHER_ERROR;
    }

    value = storedValue;
    this->m_sequenceNumberArmed = true;
    return Os::File::Status::OP_OK;
}

Os::File::Status TcSecurityDeframer ::writeSequenceNumber(const U32 value) {
    const U64 record = (static_cast<U64>(value) << 32) | static_cast<U64>(~value);
    Os::File::Status status = Utilities::FileHelper::writeToFile(this->m_sequenceNumberFilePath.toChar(), record);
    if (status != Os::File::OP_OK) {
        // Log the failure to write the sequence number (throttled -- see writeAheadPersistIfNeeded,
        // this can now only fire at most once per SEQ_NUM_PERSIST_STRIDE accepted frames)
        this->log_WARNING_HI_SequenceNumberWriteFailed(static_cast<Os::FileStatus::T>(status));
    } else {
        // Clear throttle for sequence number write failure
        this->log_WARNING_HI_SequenceNumberWriteFailed_ThrottleClear();
    }

    return status;
}

void TcSecurityDeframer ::writeAheadPersistIfNeeded(U32 acceptedSeqNum) {
    // Only persist when the accepted sequence number has caught up to (or passed) the last
    // write-ahead high-water mark. This bounds filesystem writes to at most once every
    // SEQ_NUM_PERSIST_STRIDE accepted frames instead of once per frame (issue #461).
    if (acceptedSeqNum >= this->m_persistedHighWater) {
        // Write comfortably ahead of what we've actually seen so a burst of N-1 more accepted
        // frames doesn't require another persist before the next stride boundary.
        const U32 newHighWater = acceptedSeqNum + SEQ_NUM_PERSIST_STRIDE;
        Os::File::Status status = this->writeSequenceNumber(newHighWater);
        if (status == Os::File::OP_OK) {
            this->m_persistedHighWater = newHighWater;
        }
        // On failure, m_persistedHighWater is left unchanged so the next accepted frame retries
        // the persist (still no more often than every SEQ_NUM_PERSIST_STRIDE frames in steady
        // state, since acceptedSeqNum keeps advancing past the stale high-water mark).
    }
}

}  // namespace Components

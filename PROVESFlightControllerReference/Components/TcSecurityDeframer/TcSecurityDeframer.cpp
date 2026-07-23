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

namespace Components {

// ----------------------------------------------------------------------
// Component construction and destruction
// ----------------------------------------------------------------------

TcSecurityDeframer ::TcSecurityDeframer(const char* const compName)
    : TcSecurityDeframerComponentBase(compName),
      m_sequenceNumberFilePath(),
      m_sequenceNumber(0),
      m_sequenceNumberWindow(0),
      m_keyStoreFilePath(),
      m_keyStore(),
      m_keyIds{0} {}

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
        // Lock order: key store lock, then sequence number lock. Every other handler that takes
        // both locks (none currently do) must follow the same order to avoid deadlock.
        Os::ScopeLock keyLock(this->m_keyStoreLock);
        Os::ScopeLock seqLock(this->m_sequenceNumberLock);

        // --- Validate SPI and anti-replay sequence number ---
        PacketValidator::Status validationStatus = validatePacket(parseResult.securityHeader, this->m_sequenceNumber,
                                                                  this->m_sequenceNumberWindow, this->activeSpiSlots());

        if (validationStatus == PacketValidator::Status::SpiInvalid) {
            // The key store is shared across all TcSecurityDeframer instances (UART/LoRa/Sband).
            // A rotation issued over one link is picked up here so the others don't need their
            // own commands re-run: reload from disk once and retry before giving up.
            this->loadKeyStore();
            validationStatus = validatePacket(parseResult.securityHeader, this->m_sequenceNumber,
                                              this->m_sequenceNumberWindow, this->activeSpiSlots());
        }

        if (validationStatus == PacketValidator::Status::SpiInvalid) {
            this->log_WARNING_HI_SpiInvalid(parseResult.securityHeader.spi);
        } else if (validationStatus == PacketValidator::Status::SequenceNumberInvalid) {
            this->log_WARNING_HI_SequenceNumberInvalid(parseResult.securityHeader.sequenceNumber,
                                                       this->m_sequenceNumber, this->m_sequenceNumberWindow);
        } else {
            this->log_WARNING_HI_SpiInvalid_ThrottleClear();
            this->log_WARNING_HI_SequenceNumberInvalid_ThrottleClear();

            uint32_t hmacKeyId = 0;
            const bool keyFound = this->findKeyIdForSpi(parseResult.securityHeader.spi, hmacKeyId);

            // --- Authenticate: HMAC over Security Header + Data Field ---
            const PacketAuthenticator::AuthenticationResult authResult =
                keyFound
                    ? authenticatePacket(data.getData(), data.getSize(), parseResult.securityTrailer.mac, hmacKeyId)
                    : PacketAuthenticator::AuthenticationResult{PacketAuthenticator::AuthenticationStatus::VerifyError,
                                                                0};

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

void TcSecurityDeframer ::PROVISION_KEY_cmdHandler(FwOpcodeType opCode,
                                                   U32 cmdSeq,
                                                   U16 spi,
                                                   const Fw::CmdStringArg& key) {
    Os::ScopeLock lock(this->m_keyStoreLock);

    // PROVISION_KEY is trust-on-first-use bootstrap: only honored while the store is empty.
    // Once any key exists, rotation must go through ADD_KEY/REMOVE_KEY (which require auth).
    if (this->activeKeyCount() != 0) {
        this->log_WARNING_HI_KeyProvisionFailed(KeyStoreProvisionStatus::NotEmpty);
        this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::EXECUTION_ERROR);
        return;
    }

    uint8_t keyBytes[Ccsds355_0_B_2::kTCSecurityTrailer];
    if (!parseHexKey(key.toChar(), keyBytes)) {
        this->log_WARNING_HI_KeyProvisionFailed(KeyStoreProvisionStatus::ParseKeyError);
        this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::EXECUTION_ERROR);
        return;
    }

    this->m_keyStore[0].set_valid(true);
    this->m_keyStore[0].set_spi(spi);
    this->m_keyStore[0].set_key(keyBytes);

    if (this->writeKeyStore() != Os::File::OP_OK) {
        this->m_keyStore[0].set_valid(false);
        this->log_WARNING_HI_KeyProvisionFailed(KeyStoreProvisionStatus::WriteError);
        this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::EXECUTION_ERROR);
        return;
    }

    this->importKeyStore();
    this->tlmWrite_ActiveKeyCount(this->activeKeyCount());
    this->log_ACTIVITY_HI_KeyProvisioned(spi);
    this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
}

void TcSecurityDeframer ::ADD_KEY_cmdHandler(FwOpcodeType opCode, U32 cmdSeq, U16 spi, const Fw::CmdStringArg& key) {
    Os::ScopeLock lock(this->m_keyStoreLock);

    const U8 count = this->activeKeyCount();
    if (count >= AuthKeyStore::SIZE) {
        this->log_WARNING_HI_KeyAddFailed(KeyStoreProvisionStatus::StoreFull);
        this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::EXECUTION_ERROR);
        return;
    }

    uint8_t keyBytes[Ccsds355_0_B_2::kTCSecurityTrailer];
    if (!parseHexKey(key.toChar(), keyBytes)) {
        this->log_WARNING_HI_KeyAddFailed(KeyStoreProvisionStatus::ParseKeyError);
        this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::EXECUTION_ERROR);
        return;
    }

    // Find the first empty slot for the new key
    U32 emptySlot = AuthKeyStore::SIZE;
    for (U32 i = 0; i < AuthKeyStore::SIZE; i++) {
        if (!this->m_keyStore[i].get_valid()) {
            emptySlot = i;
            break;
        }
    }
    FW_ASSERT(emptySlot < AuthKeyStore::SIZE);

    this->m_keyStore[emptySlot].set_valid(true);
    this->m_keyStore[emptySlot].set_spi(spi);
    this->m_keyStore[emptySlot].set_key(keyBytes);

    if (this->writeKeyStore() != Os::File::OP_OK) {
        this->m_keyStore[emptySlot].set_valid(false);
        this->log_WARNING_HI_KeyAddFailed(KeyStoreProvisionStatus::WriteError);
        this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::EXECUTION_ERROR);
        return;
    }

    this->importKeyStore();
    this->tlmWrite_ActiveKeyCount(this->activeKeyCount());
    this->log_ACTIVITY_HI_KeyAdded(spi);
    this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
}

void TcSecurityDeframer ::REMOVE_KEY_cmdHandler(FwOpcodeType opCode, U32 cmdSeq, U16 spi) {
    Os::ScopeLock lock(this->m_keyStoreLock);

    if (this->activeKeyCount() <= 1) {
        this->log_WARNING_HI_KeyRemoveFailed(KeyStoreProvisionStatus::LastKey);
        this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::EXECUTION_ERROR);
        return;
    }

    U32 targetSlot = AuthKeyStore::SIZE;
    for (U32 i = 0; i < AuthKeyStore::SIZE; i++) {
        if (this->m_keyStore[i].get_valid() && this->m_keyStore[i].get_spi() == spi) {
            targetSlot = i;
            break;
        }
    }

    if (targetSlot >= AuthKeyStore::SIZE) {
        this->log_WARNING_HI_KeyRemoveFailed(KeyStoreProvisionStatus::SpiNotFound);
        this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::EXECUTION_ERROR);
        return;
    }

    this->m_keyStore[targetSlot].set_valid(false);

    if (this->writeKeyStore() != Os::File::OP_OK) {
        this->m_keyStore[targetSlot].set_valid(true);
        this->log_WARNING_HI_KeyRemoveFailed(KeyStoreProvisionStatus::WriteError);
        this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::EXECUTION_ERROR);
        return;
    }

    this->importKeyStore();
    this->tlmWrite_ActiveKeyCount(this->activeKeyCount());
    this->log_ACTIVITY_HI_KeyRemoved(spi);
    this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
}

// ----------------------------------------------------------------------
// Public helper methods
// ----------------------------------------------------------------------

void TcSecurityDeframer ::configure() {
    Fw::ParamValid is_valid;

    {
        Os::ScopeLock lock(this->m_sequenceNumberLock);

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
    }

    {
        Os::ScopeLock lock(this->m_keyStoreLock);

        // Get the key store file path from the parameter
        this->m_keyStoreFilePath = this->paramGet_KEY_STORE_FILE_PATH(is_valid);
        FW_ASSERT(is_valid == Fw::ParamValid::VALID || is_valid == Fw::ParamValid::DEFAULT);

        // Load the key store from the file system and import any valid keys into PSA. On a
        // missing/unreadable store (already evented by loadKeyStore), m_keyStore is left with
        // no valid slots: a keyless board must still boot so it can be provisioned.
        (void)this->loadKeyStore();
        this->tlmWrite_ActiveKeyCount(this->activeKeyCount());
    }
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

Os::File::Status TcSecurityDeframer ::loadKeyStore() {
    AuthKeyStore store;
    Os::File::Status status = Utilities::FileHelper::readFromFile(this->m_keyStoreFilePath.toChar(), store);
    if (status == Os::File::OP_OK) {
        this->m_keyStore = store;
        this->log_WARNING_HI_KeyStoreReadFailed_ThrottleClear();
    } else if (status == Os::File::DOESNT_EXIST) {
        // No store file yet: keyless state. A keyless board must still boot so it can be
        // provisioned; leave m_keyStore at its default (no valid slots).
        this->m_keyStore = AuthKeyStore();
        this->log_WARNING_HI_KeyStoreReadFailed_ThrottleClear();
    } else {
        this->log_WARNING_HI_KeyStoreReadFailed(static_cast<Os::FileStatus::T>(status));
    }

    this->importKeyStore();
    return status;
}

Os::File::Status TcSecurityDeframer ::writeKeyStore() {
    Os::File::Status status = Utilities::FileHelper::writeToFile(this->m_keyStoreFilePath.toChar(), this->m_keyStore);
    if (status != Os::File::OP_OK) {
        this->log_WARNING_HI_KeyStoreWriteFailed(static_cast<Os::FileStatus::T>(status));
    } else {
        this->log_WARNING_HI_KeyStoreWriteFailed_ThrottleClear();
    }

    return status;
}

void TcSecurityDeframer ::importKeyStore() {
    // Release any previously-imported keys before re-importing, so rotation (and reloads that
    // pick up another link's rotation) never leaves a stale key importable in PSA.
    for (U32 i = 0; i < AuthKeyStore::SIZE; i++) {
        if (this->m_keyIds[i] != 0) {
            destroyHmacKey(this->m_keyIds[i]);
            this->m_keyIds[i] = 0;
        }
    }

    for (U32 i = 0; i < AuthKeyStore::SIZE; i++) {
        if (!this->m_keyStore[i].get_valid()) {
            continue;
        }

        uint32_t keyId = 0;
        const PacketAuthenticator::KeyImportResult result = importHmacKeyBytes(this->m_keyStore[i].get_key(), keyId);
        if (result.status == PacketAuthenticator::KeyImportStatus::Success) {
            this->m_keyIds[i] = keyId;
        }
        // On import failure the slot stays without a usable PSA key id; the store on disk is
        // unaffected, so a subsequent reload/rotation can recover once the underlying PSA issue
        // clears.
    }
}

bool TcSecurityDeframer ::findKeyIdForSpi(uint32_t spi, uint32_t& keyId) const {
    for (U32 i = 0; i < AuthKeyStore::SIZE; i++) {
        if (this->m_keyStore[i].get_valid() && this->m_keyStore[i].get_spi() == spi && this->m_keyIds[i] != 0) {
            keyId = this->m_keyIds[i];
            return true;
        }
    }
    return false;
}

U8 TcSecurityDeframer ::activeKeyCount() const {
    U8 count = 0;
    for (U32 i = 0; i < AuthKeyStore::SIZE; i++) {
        if (this->m_keyStore[i].get_valid()) {
            count++;
        }
    }
    return count;
}

ActiveSpiSlots TcSecurityDeframer ::activeSpiSlots() const {
    static_assert(AuthKeyStore::SIZE == kMaxActiveKeys, "ActiveSpiSlots must match AuthKeyStore::SIZE");
    ActiveSpiSlots slots{};
    for (U32 i = 0; i < AuthKeyStore::SIZE; i++) {
        slots[i].valid = this->m_keyStore[i].get_valid();
        slots[i].spi = this->m_keyStore[i].get_spi();
    }
    return slots;
}

}  // namespace Components

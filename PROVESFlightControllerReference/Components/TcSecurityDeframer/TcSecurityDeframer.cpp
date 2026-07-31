// ======================================================================
// \title  TcSecurityDeframer.cpp
// \brief  cpp file for TcSecurityDeframer component implementation class
// ======================================================================

#include "PROVESFlightControllerReference/Components/TcSecurityDeframer/TcSecurityDeframer.hpp"

#include <mbedtls/platform_util.h>

#include <FprimeExtras/Utilities/FileHelper/FileHelper.hpp>
#include <Fw/Log/LogString.hpp>
#include <Os/FileSystem.hpp>
#include <cstring>
#include <limits>
#include <utility>

#include "Authenticator.hpp"
#include "TcSecurityDeframer.hpp"
#include "Types.hpp"

namespace Components {

namespace {

//! Mutex protecting the key store, shared by every TcSecurityDeframer instance.
//!
//! All three instances (UART/LoRa/Sband) back onto a single key store file, so this cannot be a
//! per-instance member: without a shared lock, a PROVISION_KEY/ADD_KEY on one instance's command
//! thread can rewrite the file while another instance's dataIn_handler is reading it on the com
//! thread, which at worst yields a half-old/half-new fixed-layout record whose valid byte is set
//! but whose key bytes are mixed. It also guards each instance's in-memory m_keyStore/m_keyIds,
//! which is per-instance state; one lock covering both is sufficient and keeps the lock order
//! simple, since contention here is limited to key rotation and SPI misses.
//!
//! Function-local static so initialization order relative to component construction is defined.
Os::Mutex& keyStoreLock() {
    static Os::Mutex lock;
    return lock;
}

//! Counter bumped by every successful writeKeyStore(), read under keyStoreLock().
//!
//! An instance whose in-memory copy is at the current generation knows no one has rewritten the
//! store since it last read, so an unknown SPI really is unknown and needs no flash read. Without
//! this, any unauthenticated frame carrying a bogus SPI - SPI validation runs before MAC
//! verification - would drive a littlefs read plus a full PSA destroy/re-import of every slot, on
//! the com thread, holding the lock shared by all three uplinks.
U32& keyStoreGeneration() {
    static U32 generation = 0;
    return generation;
}

//! Asks the filesystem holding `filePath` whether it is actually mounted.
//!
//! fs_statvfs resolves the mount point and queries the underlying FS, so a success is direct
//! evidence that /keys is up. This is the signal that makes "stat says the file is missing" safe to
//! act on: on Zephyr, fs_stat returns -ENOENT for an unmounted mount point just as it does for a
//! missing file (subsys/fs/fs.c: fs_get_mnt_point), so absence alone proves nothing.
KeyStore::MountProbe probeMount(const char* filePath) {
    char directory[64];
    const char* const lastSlash = std::strrchr(filePath, '/');
    if (lastSlash == nullptr || lastSlash == filePath) {
        directory[0] = '/';
        directory[1] = '\0';
    } else {
        const size_t length = static_cast<size_t>(lastSlash - filePath);
        if (length >= sizeof directory) {
            return KeyStore::MountProbe::Unknown;
        }
        std::memcpy(directory, filePath, length);
        directory[length] = '\0';
    }

    FwSizeType totalBytes = 0;
    FwSizeType freeBytes = 0;
    return (Os::FileSystem::getFreeSpace(directory, totalBytes, freeBytes) == Os::FileSystem::OP_OK)
               ? KeyStore::MountProbe::Live
               : KeyStore::MountProbe::Unknown;
}

}  // namespace

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
      m_keyIds{0},
      // Sentinel: no generation ever takes this value, so the first unknown-SPI check reloads even
      // if configure() has not run.
      m_keyStoreGeneration(std::numeric_limits<U32>::max()) {}

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
        Os::ScopeLock keyLock(keyStoreLock());
        Os::ScopeLock seqLock(this->m_sequenceNumberLock);

        // --- Validate SPI and anti-replay sequence number ---
        PacketValidator::Status validationStatus = validatePacket(parseResult.securityHeader, this->m_sequenceNumber,
                                                                  this->m_sequenceNumberWindow, this->activeSpiSlots());

        if (validationStatus == PacketValidator::Status::SpiInvalid &&
            this->m_keyStoreGeneration != keyStoreGeneration()) {
            // The key store is shared across all TcSecurityDeframer instances (UART/LoRa/Sband).
            // A rotation issued over one link is picked up here so the others don't need their
            // own commands re-run: reload from disk once and retry before giving up.
            //
            // Gated on the generation counter because this path is reachable by an unauthenticated
            // frame: SPI validation runs before MAC verification, so without the gate a stream of
            // bogus-SPI frames would serialize all three uplinks behind a flash read and a full PSA
            // re-import per frame. Every writer of the store is in this process and bumps the
            // counter under the same lock, so the reload is exact and free in the common case.
            (void)this->loadKeyStore();
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
    Os::ScopeLock lock(keyStoreLock());

    // Refresh from disk before inspecting the store: this instance's in-memory copy may predate a
    // rotation issued over another link, and the trust-on-first-use check below is only meaningful
    // against the store that is actually on flash.
    const Os::File::Status loadStatus = this->loadKeyStore();

    // Trust-on-first-use must be gated on proof that the store is empty, not merely on the absence
    // of a key we managed to read. PROVISION_KEY is bypass-allowlisted so it works on a keyless
    // board, so if an unreadable store (a truncated record reading back as BAD_SIZE, a littlefs
    // error, a mount not ready yet) counted as "keyless", anyone in radio range could induce a read
    // failure and install their own key while a valid one still sits on flash.
    //
    // The proof cannot come from loadStatus alone: on the Zephyr target an absent file reports
    // OTHER_ERROR, not DOESNT_EXIST, so gating on the status made a factory-fresh (or /keys-erased)
    // board refuse its own bootstrap forever - keyless and unprovisionable, i.e. total command
    // loss. probeKeyStore() asks the filesystem instead. See Components::KeyStore in Types.hpp.
    KeyStore::MountProbe mountProbe = KeyStore::MountProbe::Unknown;
    KeyStore::StoreProbe storeProbe = KeyStore::StoreProbe::Unreadable;
    this->probeKeyStore(loadStatus, mountProbe, storeProbe);

    if (!KeyStore::storeStateIsKnown(mountProbe, storeProbe)) {
        this->log_WARNING_HI_KeyProvisionFailed(KeyStoreProvisionStatus::StoreUnreadable);
        this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::EXECUTION_ERROR);
        return;
    }

    // PROVISION_KEY is trust-on-first-use bootstrap: only honored while the store is empty.
    // Once any key exists, rotation must go through ADD_KEY/REMOVE_KEY (which require auth).
    if (!KeyStore::storeIsProvisionable(mountProbe, storeProbe, this->activeKeyCount())) {
        this->log_WARNING_HI_KeyProvisionFailed(KeyStoreProvisionStatus::NotEmpty);
        this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::EXECUTION_ERROR);
        return;
    }

    uint8_t keyBytes[Ccsds355_0_B_2::kTCSecurityTrailer];
    if (!parseHexKey(key.toChar(), keyBytes)) {
        // parseHexKey fills the buffer as it scans, so a key rejected part-way through leaves a
        // prefix of real key bytes behind on this stack frame.
        mbedtls_platform_zeroize(keyBytes, sizeof keyBytes);
        this->log_WARNING_HI_KeyProvisionFailed(KeyStoreProvisionStatus::ParseKeyError);
        this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::EXECUTION_ERROR);
        return;
    }

    this->m_keyStore[0].set_valid(true);
    this->m_keyStore[0].set_spi(spi);
    this->m_keyStore[0].set_key(keyBytes);
    // The key now lives in m_keyStore; drop the plaintext copy from this stack frame.
    mbedtls_platform_zeroize(keyBytes, sizeof keyBytes);

    if (this->writeKeyStore() != Os::File::OP_OK) {
        this->m_keyStore[0].set_valid(false);
        // Don't leave the rejected key's bytes in an invalid slot: a later successful write would
        // persist them to flash.
        mbedtls_platform_zeroize(this->m_keyStore[0].get_key(), sizeof(AuthKeySlot::Type_of_key));
        this->log_WARNING_HI_KeyProvisionFailed(KeyStoreProvisionStatus::WriteError);
        this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::EXECUTION_ERROR);
        return;
    }

    // The store is durable at this point. A PSA import failure is reported but not rolled back:
    // the key stays on flash so a reboot or a later reload can retry the import, and discarding a
    // key the operator may not be able to re-send would be the worse outcome.
    const bool imported = this->importKeyStore();
    this->tlmWrite_ActiveKeyCount(this->activeKeyCount());
    if (!imported) {
        this->log_WARNING_HI_KeyProvisionFailed(KeyStoreProvisionStatus::ImportError);
        this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::EXECUTION_ERROR);
        return;
    }

    this->log_ACTIVITY_HI_KeyProvisioned(spi);
    this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
}

void TcSecurityDeframer ::ADD_KEY_cmdHandler(FwOpcodeType opCode, U32 cmdSeq, U16 spi, const Fw::CmdStringArg& key) {
    Os::ScopeLock lock(keyStoreLock());

    // Refresh from disk before the read-modify-write below. Without this, an instance whose
    // in-memory store predates a rotation issued over another link would write its stale copy
    // back, resurrecting a revoked key on flash and dropping the current one.
    const Os::File::Status loadStatus = this->loadKeyStore();

    // If the store could not be read back, its contents are unknown, and the read-modify-write
    // below would persist a guess: writing the last in-memory copy over whatever is actually on
    // flash. Refuse rather than risk dropping a live key. As in PROVISION_KEY, "could not be read"
    // has to be established by probing the filesystem, not by reading loadStatus.
    KeyStore::MountProbe mountProbe = KeyStore::MountProbe::Unknown;
    KeyStore::StoreProbe storeProbe = KeyStore::StoreProbe::Unreadable;
    this->probeKeyStore(loadStatus, mountProbe, storeProbe);

    if (!KeyStore::storeStateIsKnown(mountProbe, storeProbe)) {
        this->log_WARNING_HI_KeyAddFailed(KeyStoreProvisionStatus::StoreUnreadable);
        this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::EXECUTION_ERROR);
        return;
    }

    const U8 count = this->activeKeyCount();
    if (count >= AuthKeyStore::SIZE) {
        this->log_WARNING_HI_KeyAddFailed(KeyStoreProvisionStatus::StoreFull);
        this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::EXECUTION_ERROR);
        return;
    }

    // Two slots with the same SPI are unusable: findKeyIdForSpi only ever returns the first match,
    // so the new key would authenticate nothing while REMOVE_KEY would clear only one of the two.
    if (this->hasSpi(spi)) {
        this->log_WARNING_HI_KeyAddFailed(KeyStoreProvisionStatus::DuplicateSpi);
        this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::EXECUTION_ERROR);
        return;
    }

    uint8_t keyBytes[Ccsds355_0_B_2::kTCSecurityTrailer];
    if (!parseHexKey(key.toChar(), keyBytes)) {
        // See PROVISION_KEY: a partially-parsed key leaves real key bytes on the stack.
        mbedtls_platform_zeroize(keyBytes, sizeof keyBytes);
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
    // The key now lives in m_keyStore; drop the plaintext copy from this stack frame.
    mbedtls_platform_zeroize(keyBytes, sizeof keyBytes);

    if (this->writeKeyStore() != Os::File::OP_OK) {
        this->m_keyStore[emptySlot].set_valid(false);
        // See PROVISION_KEY: an invalid slot must not carry key bytes into a later write.
        mbedtls_platform_zeroize(this->m_keyStore[emptySlot].get_key(), sizeof(AuthKeySlot::Type_of_key));
        this->log_WARNING_HI_KeyAddFailed(KeyStoreProvisionStatus::WriteError);
        this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::EXECUTION_ERROR);
        return;
    }

    // See PROVISION_KEY: a durable store with a failed import is reported, not rolled back.
    const bool imported = this->importKeyStore();
    this->tlmWrite_ActiveKeyCount(this->activeKeyCount());
    if (!imported) {
        this->log_WARNING_HI_KeyAddFailed(KeyStoreProvisionStatus::ImportError);
        this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::EXECUTION_ERROR);
        return;
    }

    this->log_ACTIVITY_HI_KeyAdded(spi);
    this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
}

void TcSecurityDeframer ::REMOVE_KEY_cmdHandler(FwOpcodeType opCode, U32 cmdSeq, U16 spi) {
    Os::ScopeLock lock(keyStoreLock());

    // Refresh from disk before the read-modify-write below, for the same reason as ADD_KEY. This
    // also keeps the LastKey and SpiNotFound rejections below truthful against the current store
    // rather than a stale copy.
    const Os::File::Status loadStatus = this->loadKeyStore();

    // See ADD_KEY: an unreadable store makes the read-modify-write a guess.
    KeyStore::MountProbe mountProbe = KeyStore::MountProbe::Unknown;
    KeyStore::StoreProbe storeProbe = KeyStore::StoreProbe::Unreadable;
    this->probeKeyStore(loadStatus, mountProbe, storeProbe);

    if (!KeyStore::storeStateIsKnown(mountProbe, storeProbe)) {
        this->log_WARNING_HI_KeyRemoveFailed(KeyStoreProvisionStatus::StoreUnreadable);
        this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::EXECUTION_ERROR);
        return;
    }

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

    // Clearing `valid` alone would leave the revoked key's raw bytes in the fixed-layout record on
    // flash, recoverable by anyone who can read the partition. Wipe the slot's key material too,
    // keeping a copy only long enough to restore the slot if the durable write fails.
    uint8_t revokedKey[Ccsds355_0_B_2::kTCSecurityTrailer];
    static_assert(sizeof(revokedKey) == sizeof(AuthKeySlot::Type_of_key), "key slot size mismatch");
    std::memcpy(revokedKey, this->m_keyStore[targetSlot].get_key(), sizeof revokedKey);

    this->m_keyStore[targetSlot].set_valid(false);
    mbedtls_platform_zeroize(this->m_keyStore[targetSlot].get_key(), sizeof(AuthKeySlot::Type_of_key));

    const Os::File::Status writeStatus = this->writeKeyStore();
    if (writeStatus != Os::File::OP_OK) {
        this->m_keyStore[targetSlot].set_key(revokedKey);
        this->m_keyStore[targetSlot].set_valid(true);
    }
    mbedtls_platform_zeroize(revokedKey, sizeof revokedKey);

    if (writeStatus != Os::File::OP_OK) {
        this->log_WARNING_HI_KeyRemoveFailed(KeyStoreProvisionStatus::WriteError);
        this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::EXECUTION_ERROR);
        return;
    }

    // See PROVISION_KEY: a durable store with a failed import is reported, not rolled back. On
    // removal the failure can only concern the surviving slot(s), which are re-imported here.
    const bool imported = this->importKeyStore();
    this->tlmWrite_ActiveKeyCount(this->activeKeyCount());
    if (!imported) {
        this->log_WARNING_HI_KeyRemoveFailed(KeyStoreProvisionStatus::ImportError);
        this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::EXECUTION_ERROR);
        return;
    }

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
        Os::ScopeLock lock(keyStoreLock());

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

    // m_keyStore now reflects the store as of this generation, whether or not the read succeeded:
    // on a failure a retry would read the same bytes, so re-reading per frame buys nothing. The
    // next writer bumps the generation and forces a fresh attempt.
    this->m_keyStoreGeneration = keyStoreGeneration();

    // Import failures are surfaced to the operator by the command handlers, which call
    // importKeyStore() directly; a reload has no command context to report into.
    (void)this->importKeyStore();
    return status;
}

void TcSecurityDeframer ::probeKeyStore(const Os::File::Status loadStatus,
                                        KeyStore::MountProbe& mount,
                                        KeyStore::StoreProbe& store) const {
    if (loadStatus == Os::File::OP_OK) {
        // A full, successful read is self-evidently proof the filesystem served the file.
        mount = KeyStore::MountProbe::Live;
        store = KeyStore::StoreProbe::Present;
        return;
    }

    // The read failed. Do not infer why from its status: ZephyrFile::open collapses every fs_open
    // errno into OTHER_ERROR, so on flight hardware a missing file and a corrupt filesystem are
    // indistinguishable here. Ask the filesystem directly instead.
    //
    // Os::FileSystem::getPathType() is deliberately not used: it folds every error into NOT_EXIST,
    // which would turn an I/O error into a false "absent" and re-open the very hole this gate
    // exists to close. Go through the interface to keep the real status.
    Os::FileSystem::PathType pathType = Os::FileSystem::PathType::NOT_EXIST;
    const Os::FileSystem::Status statStatus =
        Os::FileSystem::getSingleton()._getPathType(this->m_keyStoreFilePath.toChar(), pathType);

    // DOESNT_EXIST from stat is a positive answer ("this path is not there"), but on Zephyr it is
    // also what an unmounted /keys reports, so it only counts once probeMount() corroborates it.
    store =
        (statStatus == Os::FileSystem::DOESNT_EXIST) ? KeyStore::StoreProbe::Absent : KeyStore::StoreProbe::Unreadable;
    mount = probeMount(this->m_keyStoreFilePath.toChar());
}

Os::File::Status TcSecurityDeframer ::writeKeyStore() {
    // Write to a temp file, flush, then rename over the target - the same pattern as
    // StartupManager::persist_boot_count, and for a more serious failure mode. Overwriting the
    // live store in place meant a reset mid-write (e.g. the watchdog power cycle used for
    // command-loss recovery) could leave a truncated file, which reads back as BAD_SIZE and boots
    // the board keyless with no authenticated way back in. The rename is not guaranteed power-cut
    // atomic on the flight FS, but the new store is fully written and flushed before it replaces
    // the old one, so the worst case shrinks to a missing file during the rename window.
    Fw::String tempPath(this->m_keyStoreFilePath);
    tempPath += ".tmp";

    Os::File file;
    Os::File::Status status =
        file.open(tempPath.toChar(), Os::File::Mode::OPEN_CREATE, Os::File::OverwriteType::OVERWRITE);
    if (status == Os::File::OP_OK) {
        status = Utilities::FileHelper::writeToFile(file, this->m_keyStore);
        if (status == Os::File::OP_OK) {
            // close() returns void and cannot report a flush failure, so flush explicitly before
            // the rename makes the new store authoritative.
            status = file.flush();
        }
        (void)file.close();
    }

    if (status == Os::File::OP_OK &&
        Os::FileSystem::rename(tempPath.toChar(), this->m_keyStoreFilePath.toChar()) != Os::FileSystem::OP_OK) {
        status = Os::File::OTHER_ERROR;
    }

    if (status != Os::File::OP_OK) {
        this->log_WARNING_HI_KeyStoreWriteFailed(static_cast<Os::FileStatus::T>(status));
    } else {
        this->log_WARNING_HI_KeyStoreWriteFailed_ThrottleClear();
        // Tell the other instances their copy is stale, and record that ours is not: m_keyStore is
        // exactly what was just written.
        keyStoreGeneration()++;
        this->m_keyStoreGeneration = keyStoreGeneration();
    }

    return status;
}

bool TcSecurityDeframer ::importKeyStore() {
    bool allImported = true;

    // Release any previously-imported keys before re-importing, so rotation (and reloads that
    // pick up another link's rotation) never leaves a stale key importable in PSA.
    for (U32 i = 0; i < AuthKeyStore::SIZE; i++) {
        if (this->m_keyIds[i] != 0) {
            // A failed destroy leaves the old key live in PSA, so the slot is not actually
            // recycled. Nothing more can be done here, but the caller must not report success:
            // on REMOVE_KEY in particular that would tell the operator a key is revoked when it
            // can still authenticate frames.
            if (destroyHmacKey(this->m_keyIds[i]) != PacketAuthenticator::kPsaSuccess) {
                allImported = false;
            }
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
        } else {
            // The slot stays without a usable PSA key id, so findKeyIdForSpi will miss it and every
            // frame for that SPI fails authentication. The store on disk is unaffected, so a
            // subsequent reload/rotation can recover once the underlying PSA issue clears - but the
            // caller must not report success, or the operator would believe a key is live that the
            // board cannot actually use.
            allImported = false;
        }
    }

    return allImported;
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

bool TcSecurityDeframer ::hasSpi(uint16_t spi) const {
    for (U32 i = 0; i < AuthKeyStore::SIZE; i++) {
        if (this->m_keyStore[i].get_valid() && this->m_keyStore[i].get_spi() == spi) {
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

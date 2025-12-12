// ======================================================================
// \title  Authenticate.cpp
// \author Ines
// \brief  cpp file for Authenticate component implementation class
// ======================================================================

#include "FprimeZephyrReference/Components/Authenticate/Authenticate.hpp"

#include <psa/crypto.h>

#include <Fw/Log/LogString.hpp>
#include <cctype>
#include <cstdint>
#include <cstring>
#include <functional>
#include <iomanip>
#include <lib/fprime-extras/FprimeExtras/Utilities/FileHelper/FileHelper.hpp>
#include <sstream>

#include <zephyr/kernel.h>

// Include generated header with default key (generated at build time)
#include "AuthDefaultKey.h"

// Hardcoded Dictionary of Authentication Types
constexpr const char DEFAULT_AUTHENTICATION_TYPE[] = "HMAC";
constexpr const char DEFAULT_AUTHENTICATION_KEY[] = AUTH_DEFAULT_KEY;
constexpr const char SEQUENCE_NUMBER_PATH[] = "//sequence_number.txt";
constexpr const int SECURITY_HEADER_LENGTH = 6;
constexpr const int SECURITY_TRAILER_LENGTH = 16;
constexpr const int SPI_DEFAULT = 0;

// TO DO: ADD TO THE DOWNLINK PATH FOR LORA AND S BAND AS WELL
// TO DO GIVE THE CHOICE FOR NOT JUST HMAC BUT ALSO OTHER AUTHENTICATION TYPES

namespace Components {
// ----------------------------------------------------------------------
// Component construction and destruction
// ----------------------------------------------------------------------

Authenticate ::Authenticate(const char* const compName) : AuthenticateComponentBase(compName), sequenceNumber(0) {}

void Authenticate::init(FwEnumStoreType instance) {
    // call init from the base class
    AuthenticateComponentBase::init(instance);

    // init the sequence number
    U32 sequenceNumber = this->readSequenceNumber(SEQUENCE_NUMBER_PATH);

    this->sequenceNumber.store(sequenceNumber);
    this->tlmWrite_CurrentSequenceNumber(sequenceNumber);
}

// Reading a U32 from a file
U32 Authenticate::readSequenceNumber(const char* filepath) {
    U32 value = 0;
    U8 bufferData[sizeof(U32)];
    Fw::Buffer buffer(bufferData, sizeof(U32), 0);

    Os::File::Status status = Utilities::FileHelper::readFromFile(filepath, buffer);
    if (status == Os::File::Status::OP_OK) {
        // Copy the data from buffer to U32
        std::memcpy(&value, buffer.getData(), sizeof(U32));
    } else {
        // Copy value to buffer (to) avoid reinterpret_cast)
        std::memcpy(bufferData, &value, sizeof(U32));
        Utilities::FileHelper::writeToFile(filepath, Fw::Buffer(bufferData, sizeof(U32), 0));
    }
    return value;
}

U32 Authenticate::writeSequenceNumber(const char* filepath, U32 value) {
    // Copy value to buffer to avoid type punning
    U8 bufferData[sizeof(U32)];
    std::memcpy(bufferData, &value, sizeof(U32));
    Utilities::FileHelper::writeToFile(filepath, Fw::Buffer(bufferData, sizeof(U32), 0));
    return value;
}

void Authenticate::rejectPacket(Fw::Buffer& data, ComCfg::FrameContext& contextOut) {
    U32 newCount = this->m_rejectedPacketsCount.fetch_add(1) + 1;
    this->tlmWrite_RejectedPacketsCount(newCount);
    contextOut.set_authenticated(0);
    this->dataOut_out(0, data, contextOut);
}

Authenticate ::~Authenticate() {}

// ----------------------------------------------------------------------
// Handler implementations for typed input ports
// ----------------------------------------------------------------------

Fw::Buffer Authenticate::computeHMAC(const U8* data, const FwSizeType dataLength, const Fw::String& key) {
    // Initialize PSA crypto (idempotent, safe to call multiple times)
    psa_status_t status = psa_crypto_init();
    if (status != PSA_SUCCESS) {
        U32 statusU32 = static_cast<U32>(status);
        this->log_WARNING_HI_CryptoComputationError(statusU32);
        return Fw::Buffer();
    }

    // Parse key from hex string (32 hex characters = 16 bytes)
    const char* keyStr = key.toChar();
    FwSizeType keyLen = key.length();

    printk("[DEBUG] computeHMAC: Input key string='%s', length=%llu\n", keyStr,
           static_cast<unsigned long long>(keyLen));

    // Key must be 32 hex characters (16 bytes)
    if (keyLen != 32) {
        printk("[DEBUG] computeHMAC: ERROR - Key length is %llu, expected 32 hex characters (16 bytes)\n",
               static_cast<unsigned long long>(keyLen));
        this->log_WARNING_HI_InvalidSPI(-1);
        return Fw::Buffer();
    }

    // Parse hex string to bytes (stack-allocated array)
    U8 keyBytes[16];
    for (FwSizeType i = 0; i < 32; i += 2) {
        char byteStr[3] = {keyStr[i], keyStr[i + 1], '\0'};
        keyBytes[i / 2] = static_cast<U8>(std::strtoul(byteStr, nullptr, 16));
    }

    printk("[DEBUG] computeHMAC: Parsed key bytes: ");
    for (size_t i = 0; i < 16; i++) {
        printk("%02X ", keyBytes[i]);
    }
    printk("\n");

    printk("[DEBUG] computeHMAC: Input data length=%llu\n", static_cast<unsigned long long>(dataLength));

    printk("[DEBUG] computeHMAC: Data (first 22 bytes): ");
    for (FwSizeType i = 0; i < dataLength && i < 22; i++) {
        printk("%02X ", data[i]);
    }
    printk("\n");

    // Implement HMAC-SHA-256 manually using PSA hash API (RFC 2104)
    // HMAC(k, m) = H(k XOR opad || H(k XOR ipad || m))
    constexpr size_t kBlockSize = 64;  // SHA-256 block size
    const U8 ipad = 0x36;
    const U8 opad = 0x5C;
    U8 macOutput[32];
    size_t macOutputLength = 0;

    // Prepare key: pad with zeros or hash if longer than block size
    U8 preparedKey[kBlockSize] = {0};
    constexpr size_t keySize = 16;  // HMAC key is always 16 bytes
    if (keySize <= kBlockSize) {
        std::memcpy(preparedKey, keyBytes, keySize);
    } else {
        // Hash key if longer than block size
        psa_hash_operation_t hashOp = PSA_HASH_OPERATION_INIT;
        status = psa_hash_setup(&hashOp, PSA_ALG_SHA_256);
        if (status != PSA_SUCCESS) {
            U32 statusU32 = static_cast<U32>(status);
            this->log_WARNING_HI_CryptoComputationError(statusU32);
            return Fw::Buffer();
        }
        status = psa_hash_update(&hashOp, keyBytes, keySize);
        if (status == PSA_SUCCESS) {
            size_t hashLen = 0;
            status = psa_hash_finish(&hashOp, preparedKey, kBlockSize, &hashLen);
        }
        if (status != PSA_SUCCESS) {
            U32 statusU32 = static_cast<U32>(status);
            this->log_WARNING_HI_CryptoComputationError(statusU32);
            return Fw::Buffer();
        }
    }

    // Compute inner hash: H(k XOR ipad || m)
    U8 innerKey[kBlockSize];
    for (size_t i = 0; i < kBlockSize; i++) {
        innerKey[i] = preparedKey[i] ^ ipad;
    }

    psa_hash_operation_t innerHash = PSA_HASH_OPERATION_INIT;
    status = psa_hash_setup(&innerHash, PSA_ALG_SHA_256);
    if (status == PSA_SUCCESS) {
        status = psa_hash_update(&innerHash, innerKey, kBlockSize);
    }
    if (status == PSA_SUCCESS) {
        status = psa_hash_update(&innerHash, data, dataLength);
    }
    U8 innerHashOutput[32];
    size_t innerHashLen = 0;
    if (status == PSA_SUCCESS) {
        status = psa_hash_finish(&innerHash, innerHashOutput, sizeof(innerHashOutput), &innerHashLen);
    }

    if (status != PSA_SUCCESS) {
        U32 statusU32 = static_cast<U32>(status);
        this->log_WARNING_HI_CryptoComputationError(statusU32);
        return Fw::Buffer();
    }

    // Compute outer hash: H(k XOR opad || innerHash)
    U8 outerKey[kBlockSize];
    for (size_t i = 0; i < kBlockSize; i++) {
        outerKey[i] = preparedKey[i] ^ opad;
    }

    psa_hash_operation_t outerHash = PSA_HASH_OPERATION_INIT;
    status = psa_hash_setup(&outerHash, PSA_ALG_SHA_256);
    if (status == PSA_SUCCESS) {
        status = psa_hash_update(&outerHash, outerKey, kBlockSize);
    }
    if (status == PSA_SUCCESS) {
        status = psa_hash_update(&outerHash, innerHashOutput, innerHashLen);
    }
    if (status == PSA_SUCCESS) {
        status = psa_hash_finish(&outerHash, macOutput, sizeof(macOutput), &macOutputLength);
    }

    if (status != PSA_SUCCESS) {
        U32 statusU32 = static_cast<U32>(status);
        this->log_WARNING_HI_CryptoComputationError(statusU32);
        return Fw::Buffer();
    }

    // Allocate a buffer for the HMAC result (we only need 16 bytes per CCSDS spec)
    // But we computed 32 bytes (full SHA-256), so we'll take the first 16
    const size_t hmacOutputLength = 16;  // CCSDS 355.0-B-2 specifies 16 bytes
    U8* hmacData = new U8[hmacOutputLength];
    if (hmacData == nullptr) {
        printk("[DEBUG] computeHMAC: ERROR - Failed to allocate memory for HMAC result\n");
        return Fw::Buffer();
    }
    std::memcpy(hmacData, macOutput, hmacOutputLength);

    printk("[DEBUG] computeHMAC: Computed HMAC result (16 bytes): ");
    for (size_t i = 0; i < hmacOutputLength; i++) {
        printk("%02X ", hmacData[i]);
    }
    printk("\n");

    // Create Fw::Buffer with the HMAC data (context 0 for now)
    return Fw::Buffer(hmacData, static_cast<FwSizeType>(hmacOutputLength), 0);
}

bool Authenticate::validateSequenceNumber(U32 received, U32 expected) {
    // validate the sequence number by checking if it is within the window of the expected sequence number
    Fw::ParamValid valid;
    U32 window = paramGet_SEQ_NUM_WINDOW(valid);
    /*
     * Compute the difference between received and expected sequence numbers using unsigned
     * 32-bit arithmetic. This handles wraparound correctly due to the well-defined behavior
     * of unsigned integer overflow in C++. For example, if expected=0xFFFFFFFE and received=1,
     * then (received - expected) == 3 (modulo 2^32). This is a standard technique for
     * sequence number window validation (see RFC 1982: Serial Number Arithmetic).
     */
    const U32 delta = received - expected;
    if (delta > window) {
        this->log_WARNING_HI_SequenceNumberOutOfWindow(received, expected, window);
        return false;
    }
    return true;
}

bool Authenticate::compareHMAC(const U8* expected, const U8* actual, FwSizeType length) const {
    if (expected == nullptr || actual == nullptr) {
        return false;
    }
    return std::memcmp(expected, actual, static_cast<size_t>(length)) == 0;
}

bool Authenticate::validateHMAC(const U8* data,
                                FwSizeType dataLength,
                                const Fw::String& key,
                                const U8* securityTrailer) {
    // Compute HMAC (allocates memory)
    // data already contains security header + payload (matching Python's hmac.new(key, header + data, ...))
    Fw::Buffer computedHmac = this->computeHMAC(data, dataLength, key);
    const U8* computedHmacData = computedHmac.getData();
    const FwSizeType computedHmacLength = computedHmac.getSize();

    // Store non-const pointer for deletion (getData() returns const, but we allocated with new[])
    // This pointer is scoped to this function only - cannot be accidentally used after return
    U8* hmacDataToDelete = const_cast<U8*>(computedHmacData);

    if (computedHmacData == nullptr || computedHmacLength < 8) {
        // Clean up memory before returning (delete[] nullptr is safe)
        delete[] hmacDataToDelete;
        return false;
    }

    printk("[DEBUG] validateHMAC: Computed HMAC (first 16 bytes): ");
    for (FwSizeType i = 0; i < (computedHmacLength < 16 ? computedHmacLength : 16); i++) {
        printk("%02X ", computedHmacData[i]);
    }
    printk("\n");

    printk("[DEBUG] validateHMAC: Expected HMAC from securityTrailer (16 bytes): ");
    for (FwSizeType i = 0; i < 16; i++) {
        printk("%02X ", securityTrailer[i]);
    }
    printk("\n");

    // Compare computed HMAC with expected HMAC from security trailer
    bool hmacValid = this->compareHMAC(computedHmacData, securityTrailer, computedHmacLength);

    if (!hmacValid) {
        printk("[DEBUG] validateHMAC: HMAC comparison FAILED - computed and expected do not match\n");
    } else {
        printk("[DEBUG] validateHMAC: HMAC comparison PASSED\n");
    }

    delete[] hmacDataToDelete;

    return hmacValid;
}

bool Authenticate::validateHeader(Fw::Buffer& data,
                                  ComCfg::FrameContext& contextOut,
                                  U8* securityHeader,
                                  U8* securityTrailer,
                                  U32& spi,
                                  U32& sequenceNumber) {
    // 34 = 12 (data) + 6 (security header) + 16 (security trailer)
    // some packets will be missed here, because we don't have a clear way to tell if the packet is long because its
    // authenticating or because its not a valid packet.
    // later we will change the integration tests to all run on plugins, but for now, only the 12 bytes will be
    // authenticated.

    // Take the first 6 bytes as the security header
    std::memcpy(securityHeader, data.getData(), SECURITY_HEADER_LENGTH);
    // increment the pointer to the data to point to the rest of the packet
    data.setData(data.getData() + SECURITY_HEADER_LENGTH);
    data.setSize(data.getSize() - SECURITY_HEADER_LENGTH);

    // now we get the footer (last 16 bytes)
    std::memcpy(securityTrailer, data.getData() + data.getSize() - SECURITY_TRAILER_LENGTH, SECURITY_TRAILER_LENGTH);
    // decrement the size of the data to remove the footer
    data.setSize(data.getSize() - SECURITY_TRAILER_LENGTH);

    // the first two bytes are the SPI
    spi = (static_cast<U32>(securityHeader[0]) << 8) | static_cast<U32>(securityHeader[1]);

    // the next four bytes are the sequence number
    sequenceNumber = (static_cast<U32>(securityHeader[2]) << 24) | (static_cast<U32>(securityHeader[3]) << 16) |
                     (static_cast<U32>(securityHeader[4]) << 8) | static_cast<U32>(securityHeader[5]);

    return true;
}

void Authenticate ::dataIn_handler(FwIndexType portNum, Fw::Buffer& data, const ComCfg::FrameContext& context) {
    ComCfg::FrameContext contextOut = context;
    const Fw::String& type_authn = DEFAULT_AUTHENTICATION_TYPE;
    const Fw::String& key_authn = DEFAULT_AUTHENTICATION_KEY;

    // Validate buffer size before processing
    if (data.getSize() < SECURITY_HEADER_LENGTH + SECURITY_TRAILER_LENGTH) {
        printk(
            "[DEBUG] dataIn_handler: Buffer too small! Need at least %d bytes (header=%d + trailer=%d), but buffer has "
            "%llu bytes\n",
            SECURITY_HEADER_LENGTH + SECURITY_TRAILER_LENGTH, SECURITY_HEADER_LENGTH, SECURITY_TRAILER_LENGTH,
            static_cast<unsigned long long>(data.getSize()));
        this->rejectPacket(data, contextOut);
        return;
    }

    // Extract security header and trailer (without modifying the buffer yet)
    U8 securityHeader[SECURITY_HEADER_LENGTH];
    U8 securityTrailer[SECURITY_TRAILER_LENGTH];
    U32 spi = 0;
    U32 sequenceNumber = 0;

    // Extract security header (first 6 bytes)
    std::memcpy(securityHeader, data.getData(), SECURITY_HEADER_LENGTH);

    // Extract security trailer (last 16 bytes)
    std::memcpy(securityTrailer, data.getData() + data.getSize() - SECURITY_TRAILER_LENGTH, SECURITY_TRAILER_LENGTH);

    // Extract SPI and sequence number from security header
    spi = (static_cast<U32>(securityHeader[0]) << 8) | static_cast<U32>(securityHeader[1]);
    sequenceNumber = (static_cast<U32>(securityHeader[2]) << 24) | (static_cast<U32>(securityHeader[3]) << 16) |
                     (static_cast<U32>(securityHeader[4]) << 8) | static_cast<U32>(securityHeader[5]);

    // The data to authenticate is header + payload (everything except the trailer)
    const U8* dataToAuthenticate = data.getData();
    const FwSizeType dataToAuthenticateLength = data.getSize() - SECURITY_TRAILER_LENGTH;

    // Validate HMAC - compute over security header + payload, compare with trailer
    bool hmacValid = this->validateHMAC(dataToAuthenticate, dataToAuthenticateLength, key_authn, securityTrailer);

    if (!hmacValid) {
        printk("[DEBUG] Authentication: HMAC validation FAILED for APID=%u, SPI=0x%04X (%u), SeqNum=%u\n",
               contextOut.get_apid(), spi, spi, sequenceNumber);
        this->log_WARNING_HI_InvalidHash(contextOut.get_apid(), spi, sequenceNumber);
        this->rejectPacket(data, contextOut);
        return;
    }
    printk("[DEBUG] Authentication: HMAC validation PASSED\n");

    // Now strip header and trailer from the buffer for forwarding
    data.setData(data.getData() + SECURITY_HEADER_LENGTH);
    data.setSize(data.getSize() - SECURITY_HEADER_LENGTH - SECURITY_TRAILER_LENGTH);

    // Assuming SPI 0 as default (no longer reading from spi_dict.txt)
    // Any other SPI value is invalid and should be rejected
    if (spi != SPI_DEFAULT) {
        this->log_WARNING_HI_InvalidSPI(spi);
        this->rejectPacket(data, contextOut);
        return;
    }

    // check the sequence number is valid
    U32 expectedSeqNum = this->get_SequenceNumber();

    bool sequenceNumberValid = this->validateSequenceNumber(sequenceNumber, expectedSeqNum);
    if (!sequenceNumberValid) {
        this->rejectPacket(data, contextOut);
        return;
    }

    // increment the stored sequence number
    U32 newSequenceNumber = sequenceNumber + 1;
    this->sequenceNumber.store(newSequenceNumber);
    this->tlmWrite_CurrentSequenceNumber(newSequenceNumber);

    this->log_ACTIVITY_HI_ValidHash(contextOut.get_apid(), spi, sequenceNumber);

    U32 newCount = this->m_authenticatedPacketsCount.fetch_add(1) + 1;
    this->tlmWrite_AuthenticatedPacketsCount(newCount);
    contextOut.set_authenticated(1);
    this->dataOut_out(0, data, contextOut);
}

void Authenticate ::dataReturnIn_handler(FwIndexType portNum, Fw::Buffer& data, const ComCfg::FrameContext& context) {
    this->dataReturnOut_out(0, data, context);
}

U32 Authenticate ::get_SequenceNumber() {
    U32 fileSequenceNumber = this->readSequenceNumber(SEQUENCE_NUMBER_PATH);
    return fileSequenceNumber;
}

// ----------------------------------------------------------------------
// Handler implementations for commands
// ----------------------------------------------------------------------

void Authenticate ::GET_SEQ_NUM_cmdHandler(FwOpcodeType opCode, U32 cmdSeq) {
    U32 fileSequenceNumber = this->get_SequenceNumber();

    this->log_ACTIVITY_HI_EmitSequenceNumber(fileSequenceNumber);
    this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
}

void Authenticate ::SET_SEQ_NUM_cmdHandler(FwOpcodeType opCode, U32 cmdSeq, U32 seq_num) {
    // Writes the sequence number to the file system
    this->writeSequenceNumber(SEQUENCE_NUMBER_PATH, seq_num);

    this->log_ACTIVITY_HI_SetSequenceNumberSuccess(seq_num, true);
    this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
}

}  // namespace Components

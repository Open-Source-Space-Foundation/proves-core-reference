// ======================================================================
// \title  Authenticate.cpp
// \author Ines
// \brief  cpp file for Authenticate component implementation class
// ======================================================================

#include "FprimeZephyrReference/Components/Authenticate/Authenticate.hpp"

#include <mbedtls/error.h>
#include <mbedtls/pk.h>
#include <mbedtls/rsa.h>
#include <psa/crypto.h>

#include <FprimeExtras/Utilities/FileHelper/FileHelper.hpp>
#include <Fw/Log/LogString.hpp>
#include <cstring>
#include <iomanip>

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
    Os::File::Status status = Utilities::FileHelper::readFromFile(filepath, value);
    if (status != Os::File::OP_OK) {
        Utilities::FileHelper::writeToFile(filepath, value);
    }
    return value;
}

U32 Authenticate::writeSequenceNumber(const char* filepath, U32 value) {
    // Copy value to buffer to avoid type punning
    Utilities::FileHelper::writeToFile(filepath, value);
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

bool Authenticate::computeRSA(const U8* data,
                              const FwSizeType dataLength,
                              const Fw::String& key,
                              U8* output,
                              FwSizeType outputSize) {
    // Use the public key to reveal the original hash
    // This function decrypts an RSA signature (in 'data') using the public key
    // to reveal the hash that was originally signed.
    //
    // Parameters:
    //   data: RSA signature (encrypted hash) to decrypt
    //   key: RSA public key (in DER or PEM format)
    //   output: Buffer to receive the revealed hash (typically 32 bytes for SHA-256)
    //   outputSize: Size of output buffer

    // IMRPOTANT NOTE!! THIS IS ALL DEMO CODE AN ALSO SUPER NOT SAFE TO RUN< JUST HACKING IT FOR NOW
    // DEMO PURPOSES SO WE KNOW IF WE CAN DO THESE THINGS ON A PICO2

    // Check if the output buffer is large enough to hold the hash
    if (output == nullptr || outputSize < 32) {
        // Need at least 32 bytes for SHA-256 hash output
        return false;
    }

    // Initialize PSA crypto (idempotent, safe to call multiple times)
    psa_status_t psa_status = psa_crypto_init();
    if (psa_status != PSA_SUCCESS) {
        this->log_WARNING_HI_CryptoComputationError(static_cast<U32>(psa_status));
        return false;
    }

    // Use mbedTLS directly for raw RSA operations to reveal the hash from signature
    const char* keyStr = key.toChar();
    size_t keyLength = key.length();

    mbedtls_pk_context pk;
    mbedtls_pk_init(&pk);

    // Parse the public key (supports both PEM and DER formats)
    int ret = mbedtls_pk_parse_public_key(&pk, reinterpret_cast<const unsigned char*>(keyStr),
                                          keyLength + 1);  // +1 for null terminator if PEM

    if (ret != 0) {
        // Try without null terminator (for DER format)
        ret = mbedtls_pk_parse_public_key(&pk, reinterpret_cast<const unsigned char*>(keyStr), keyLength);
    }

    if (ret != 0) {
        mbedtls_pk_free(&pk);
        this->log_WARNING_HI_CryptoComputationError(static_cast<U32>(ret));
        return false;
    }

    // Verify it's an RSA key
    if (mbedtls_pk_get_type(&pk) != MBEDTLS_PK_RSA) {
        mbedtls_pk_free(&pk);
        this->log_WARNING_HI_CryptoComputationError(static_cast<U32>(-1));
        return false;
    }

    // Get the RSA context
    mbedtls_rsa_context* rsa = mbedtls_pk_rsa(pk);

    // Perform RSA public key operation to decrypt the signature
    // This reveals the padded hash
    size_t rsaKeySize = mbedtls_rsa_get_len(rsa);
    if (dataLength != rsaKeySize) {
        mbedtls_pk_free(&pk);
        this->log_WARNING_HI_CryptoComputationError(static_cast<U32>(-1));
        return false;
    }

    // Allocate buffer for decrypted signature (padded hash)
    unsigned char* decrypted = new unsigned char[rsaKeySize];
    if (decrypted == nullptr) {
        mbedtls_pk_free(&pk);
        return false;
    }

    // Perform RSA public key operation (decrypt signature to reveal hash)
    ret = mbedtls_rsa_public(rsa, data, decrypted);

    if (ret != 0) {
        delete[] decrypted;
        mbedtls_pk_free(&pk);
        this->log_WARNING_HI_CryptoComputationError(static_cast<U32>(ret));
        return false;
    }

    // Extract the hash from PKCS#1 v1.5 padding
    // PKCS#1 v1.5 format: 00 01 FF FF ... FF 00 || ASN.1 DigestInfo || Hash
    // For SHA-256, we need to find the hash (last 32 bytes after the ASN.1 header)
    // ASN.1 DigestInfo for SHA-256 is: 30 31 30 0d 06 09 60 86 48 01 65 03 04 02 01 05 00 04 20
    const unsigned char sha256DigestInfo[] = {0x30, 0x31, 0x30, 0x0d, 0x06, 0x09, 0x60, 0x86, 0x48, 0x01,
                                              0x65, 0x03, 0x04, 0x02, 0x01, 0x05, 0x00, 0x04, 0x20};
    const size_t digestInfoLen = sizeof(sha256DigestInfo);

    // Find the start of the hash (after padding and ASN.1 header)
    size_t hashStart = rsaKeySize - 32;  // Hash is last 32 bytes
    bool found = false;

    // Check if PKCS#1 v1.5 padding is correct
    if (decrypted[0] == 0x00 && decrypted[1] == 0x01) {
        // Find the 00 separator after padding
        size_t i = 2;
        while (i < rsaKeySize - digestInfoLen - 32 && decrypted[i] == 0xFF) {
            i++;
        }
        if (i < rsaKeySize - digestInfoLen - 32 && decrypted[i] == 0x00) {
            i++;
            // Check for ASN.1 DigestInfo
            if (std::memcmp(&decrypted[i], sha256DigestInfo, digestInfoLen) == 0) {
                hashStart = i + digestInfoLen;
                found = true;
            }
        }
    }

    if (!found) {
        // Fallback: assume hash is in the last 32 bytes
        hashStart = rsaKeySize - 32;
    }

    // Copy the revealed hash to output
    std::memcpy(output, &decrypted[hashStart], 32);

    // Clean up
    delete[] decrypted;
    mbedtls_pk_free(&pk);

    return true;
}

bool Authenticate::computeHMAC(const U8* data,
                               const FwSizeType dataLength,
                               const Fw::String& key,
                               U8* output,
                               FwSizeType outputSize) {
    constexpr size_t kHmacOutputLength = 16;  // CCSDS 355.0-B-2 specifies 16 bytes

    if (output == nullptr || outputSize < kHmacOutputLength) {
        return false;
    }

    // Initialize PSA crypto (idempotent, safe to call multiple times)
    psa_status_t status = psa_crypto_init();
    if (status != PSA_SUCCESS) {
        this->log_WARNING_HI_CryptoComputationError(static_cast<U32>(status));
        return false;
    }

    // Parse key from hex string (32 hex characters = 16 bytes)
    const char* keyStr = key.toChar();
    if (key.length() != 32) {
        this->log_WARNING_HI_InvalidSPI(-1);
        return false;
    }

    // Parse hex string to bytes
    constexpr size_t kKeySize = 16;
    U8 keyBytes[kKeySize];
    for (FwSizeType i = 0; i < 32; i += 2) {
        char byteStr[3] = {keyStr[i], keyStr[i + 1], '\0'};
        keyBytes[i / 2] = static_cast<U8>(std::strtoul(byteStr, nullptr, 16));
    }

    // Implement HMAC-SHA-256 using PSA hash API (RFC 2104)
    // HMAC(k, m) = H(k XOR opad || H(k XOR ipad || m))
    constexpr size_t kBlockSize = 64;  // SHA-256 block size
    constexpr U8 kIpad = 0x36;
    constexpr U8 kOpad = 0x5C;

    // Prepare key: pad with zeros (key is always 16 bytes, which is < 64)
    U8 preparedKey[kBlockSize] = {0};
    std::memcpy(preparedKey, keyBytes, kKeySize);

    // Compute inner hash: H(k XOR ipad || m)
    U8 innerKey[kBlockSize];
    for (size_t i = 0; i < kBlockSize; i++) {
        innerKey[i] = preparedKey[i] ^ kIpad;
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
        this->log_WARNING_HI_CryptoComputationError(static_cast<U32>(status));
        return false;
    }

    // Compute outer hash: H(k XOR opad || innerHash)
    U8 outerKey[kBlockSize];
    for (size_t i = 0; i < kBlockSize; i++) {
        outerKey[i] = preparedKey[i] ^ kOpad;
    }

    psa_hash_operation_t outerHash = PSA_HASH_OPERATION_INIT;
    status = psa_hash_setup(&outerHash, PSA_ALG_SHA_256);
    if (status == PSA_SUCCESS) {
        status = psa_hash_update(&outerHash, outerKey, kBlockSize);
    }
    if (status == PSA_SUCCESS) {
        status = psa_hash_update(&outerHash, innerHashOutput, innerHashLen);
    }
    U8 macOutput[32];
    size_t macOutputLength = 0;
    if (status == PSA_SUCCESS) {
        status = psa_hash_finish(&outerHash, macOutput, sizeof(macOutput), &macOutputLength);
    }
    if (status != PSA_SUCCESS) {
        this->log_WARNING_HI_CryptoComputationError(static_cast<U32>(status));
        return false;
    }

    // Copy first 16 bytes to output buffer
    std::memcpy(output, macOutput, kHmacOutputLength);
    return true;
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
    // Compute HMAC into stack-allocated buffer
    // data already contains security header + payload (matching Python's hmac.new(key, header + data, ...))
    constexpr size_t kHmacOutputLength = 16;
    U8 computedHmac[kHmacOutputLength];

    if (!this->computeHMAC(data, dataLength, key, computedHmac, kHmacOutputLength)) {
        return false;
    }

    // Compare computed HMAC with expected HMAC from security trailer
    bool hmacValid = this->compareHMAC(computedHmac, securityTrailer, kHmacOutputLength);

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
        this->log_WARNING_HI_InvalidHash(contextOut.get_apid(), spi, sequenceNumber);
        this->rejectPacket(data, contextOut);
        return;
    }

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
    U32 expectedSeqNum = this->sequenceNumber.load();

    bool sequenceNumberValid = this->validateSequenceNumber(sequenceNumber, expectedSeqNum);
    if (!sequenceNumberValid) {
        this->rejectPacket(data, contextOut);
        return;
    }

    // increment the stored sequence number
    U32 newSequenceNumber = sequenceNumber + 1;
    this->sequenceNumber.store(newSequenceNumber);
    this->writeSequenceNumber(SEQUENCE_NUMBER_PATH, newSequenceNumber);
    this->tlmWrite_CurrentSequenceNumber(newSequenceNumber);

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

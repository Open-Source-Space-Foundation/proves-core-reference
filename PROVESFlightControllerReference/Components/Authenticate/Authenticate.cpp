// ======================================================================
// \title  Authenticate.cpp
// \brief  cpp file for Authenticate component implementation class
// ======================================================================

#include "PROVESFlightControllerReference/Components/Authenticate/Authenticate.hpp"

#include <psa/crypto.h>

#include <FprimeExtras/Utilities/FileHelper/FileHelper.hpp>
#include <Fw/Log/LogString.hpp>
#include <iomanip>

// Include generated header with default key (generated at build time)
#include "AuthDefaultKey.h"

// File path for storing the sequence number persistently
// TODO(nateinaction): Move to parameter
constexpr const char SEQUENCE_NUMBER_PATH[] = "//sequence_number.txt";

namespace Components {

// ----------------------------------------------------------------------
// Component construction and destruction
// ----------------------------------------------------------------------

Authenticate ::Authenticate(const char* const compName) : AuthenticateComponentBase(compName), m_sequenceNumber(0) {}

Authenticate ::~Authenticate() {}

// ----------------------------------------------------------------------
// Handler implementations for typed input ports
// ----------------------------------------------------------------------

void Authenticate ::dataIn_handler(FwIndexType portNum, Fw::Buffer& data, const ComCfg::FrameContext& context) {
    // playing

    uint8_t* dataBuffer = data.getData();
    uint64_t dataSize = data.getSize();

    // old stuff start

    constexpr const char DEFAULT_AUTHENTICATION_KEY[] = AUTH_DEFAULT_KEY;
    const Fw::String& keyAuthn = DEFAULT_AUTHENTICATION_KEY;
    constexpr const int SECURITY_HEADER_LENGTH = 6;
    constexpr const int SECURITY_TRAILER_LENGTH = 16;
    constexpr const int SPI_DEFAULT = 0;

    // Validate buffer size before processing
    if (data.getSize() < SECURITY_HEADER_LENGTH + SECURITY_TRAILER_LENGTH) {
        this->rejectPacket(data, context);
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
    bool hmacValid = this->validateHMAC(dataToAuthenticate, dataToAuthenticateLength, keyAuthn, securityTrailer);

    // passing just the payload in here
    bool bypassAuth = this->bypassAuth(data.getData() + SECURITY_HEADER_LENGTH,
                                       data.getSize() - SECURITY_HEADER_LENGTH - SECURITY_TRAILER_LENGTH);

    if (!bypassAuth && !hmacValid) {
        this->log_WARNING_HI_InvalidHash(context.get_apid(), spi, sequenceNumber);
        this->rejectPacket(data, context);
        return;
    }

    // Now strip header and trailer from the buffer for forwarding
    data.setData(data.getData() + SECURITY_HEADER_LENGTH);
    data.setSize(data.getSize() - SECURITY_HEADER_LENGTH - SECURITY_TRAILER_LENGTH);

    // Assuming SPI 0 as default (no longer reading from spi_dict.txt)
    // Any other SPI value is invalid and should be rejected
    if (spi != SPI_DEFAULT) {
        this->log_WARNING_HI_InvalidSPI(spi);
        this->rejectPacket(data, context);
        return;
    }

    // check the sequence number is valid
    U32 expectedSeqNum = this->m_sequenceNumber.load();

    bool sequenceNumberValid = this->validateSequenceNumber(sequenceNumber, expectedSeqNum);
    if ((!bypassAuth && !sequenceNumberValid)) {
        this->rejectPacket(data, context);
        return;
    }

    U32 newSequenceNumber = expectedSeqNum + 1;
    this->m_sequenceNumber = newSequenceNumber;
    this->writeSequenceNumber(SEQUENCE_NUMBER_PATH, newSequenceNumber);
    this->tlmWrite_CurrentSequenceNumber(newSequenceNumber);

    U32 newCount = this->m_authenticatedPacketsCount.fetch_add(1) + 1;
    this->tlmWrite_AuthenticatedPacketsCount(newCount);
    this->dataOut_out(0, data, context);
}

void Authenticate ::dataReturnIn_handler(FwIndexType portNum, Fw::Buffer& data, const ComCfg::FrameContext& context) {
    this->dataReturnOut_out(0, data, context);
}

// ----------------------------------------------------------------------
// Handler implementations for commands
// ----------------------------------------------------------------------

void Authenticate ::GET_SEQ_NUM_cmdHandler(FwOpcodeType opCode, U32 cmdSeq) {
    U32 fileSequenceNumber = this->readSequenceNumber(SEQUENCE_NUMBER_PATH);

    this->log_ACTIVITY_HI_EmitSequenceNumber(fileSequenceNumber);
    this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
}

void Authenticate ::SET_SEQ_NUM_cmdHandler(FwOpcodeType opCode, U32 cmdSeq, U32 seq_num) {
    // Writes the sequence number to the file system
    this->writeSequenceNumber(SEQUENCE_NUMBER_PATH, seq_num);

    this->log_ACTIVITY_HI_SetSequenceNumberSuccess(seq_num, true);
    this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
}

// ----------------------------------------------------------------------
// Public helper methods
// ----------------------------------------------------------------------

void Authenticate::init(FwEnumStoreType instance) {
    // Call init from the base class
    AuthenticateComponentBase::init(instance);

    // Init the sequence number
    U32 sequenceNumber = this->readSequenceNumber(SEQUENCE_NUMBER_PATH);
    this->m_sequenceNumber = sequenceNumber;
    this->tlmWrite_CurrentSequenceNumber(sequenceNumber);
}

// ----------------------------------------------------------------------
// Private helper methods
// ----------------------------------------------------------------------

U32 Authenticate::readSequenceNumber(const char* filepath) {
    U32 value = 0;
    Os::File::Status status = Utilities::FileHelper::readFromFile(filepath, value);
    if (status != Os::File::OP_OK) {
        // TODO(nateinaction): do we want to set sequence number to 0 if we fail to read it? or should we handle this
        // differently?
        // TODO(nateinaction): status of this write is never checked
        Utilities::FileHelper::writeToFile(filepath, value);
    }
    return value;
}

U32 Authenticate::writeSequenceNumber(const char* filepath, U32 value) {
    // Copy value to buffer to avoid type punning
    // TODO(nateinaction): status of this write is never checked
    Utilities::FileHelper::writeToFile(filepath, value);
    // TODO(nateinaction): do we need to return the value? It is passed in as an argument so the caller already has it
    return value;
}

bool Authenticate::bypassAuth(U8* packetBuffer, FwSizeType dataLength) {
    // we want to get the packet buffer, by removing what is inside the next step, the spacedataframer
    constexpr const U8 OP_CODE_LENGTH = 4;      // F Prime opcodes are 32-bit (4 bytes)
    constexpr const U8 OP_CODE_START = 2;       // Opcode starts at byte offset 2 in the packet buffer
    constexpr const U8 SPACE_PACKET_START = 6;  // Space Packets start at byte offset 0 in the packet buffer
    static constexpr U32 kBypassOpCodes[] = {
        0x01000000,  // no op
        0x2200B000,  // get sequence number
        0x10065000,  // amateurRadio.TELL_JOKE
    };
    constexpr size_t kBypassOpCodesArrayLength = sizeof(kBypassOpCodes) / sizeof(kBypassOpCodes[0]);

    if (packetBuffer == nullptr) {
        return false;
    }
    if (dataLength < SPACE_PACKET_START) {
        return false;
    }

    // remove the space packet header
    packetBuffer += SPACE_PACKET_START;
    dataLength -= SPACE_PACKET_START;

    // Check bounds before extracting
    if (dataLength < (OP_CODE_START + OP_CODE_LENGTH)) {
        return false;
    }

    // Extract opcode bytes

    U8 opCodeBytes[OP_CODE_LENGTH];

    std::memcpy(opCodeBytes, packetBuffer + OP_CODE_START, OP_CODE_LENGTH);

    // Combine opcode bytes into a single 32-bit value for comparison
    const U32 opCode = (static_cast<U32>(opCodeBytes[0]) << 24) | (static_cast<U32>(opCodeBytes[1]) << 16) |
                       (static_cast<U32>(opCodeBytes[2]) << 8) | static_cast<U32>(opCodeBytes[3]);

    // Check if opcode matches any in the bypass list
    for (size_t i = 0; i < kBypassOpCodesArrayLength; i++) {
        if (opCode == kBypassOpCodes[i]) {
            return true;
        }
    }

    return false;
}

void Authenticate::rejectPacket(Fw::Buffer& data, const ComCfg::FrameContext& context) {
    this->log_WARNING_HI_PacketRejected();
    U32 newCount = this->m_rejectedPacketsCount.fetch_add(1) + 1;
    this->tlmWrite_RejectedPacketsCount(newCount);

    // Copy the context and set authenticated to false for the rejected packet
    ComCfg::FrameContext contextOut = context;
    contextOut.set_authenticated(false);

    // if the packet is rejected we no longer pass it down the comm stack

    this->dataReturnOut_out(0, data, contextOut);
}

bool Authenticate::computeHMAC(const U8* data,
                               const FwSizeType dataLength,
                               const Fw::String& key,
                               U8* output,
                               FwSizeType outputSize) {
    constexpr size_t kHmacOutputLength = 16;  // CCSDS 355.0-B-2 specifies 16 bytes

    // TODO(nateinaction): hmmm outputSize < kHmacOutputLength compares two constants both of size 16
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
    if (status != PSA_SUCCESS) {
        this->log_WARNING_HI_CryptoComputationError(static_cast<U32>(status));
        return false;
    }

    status = psa_hash_update(&innerHash, innerKey, kBlockSize);
    if (status != PSA_SUCCESS) {
        this->log_WARNING_HI_CryptoComputationError(static_cast<U32>(status));
        return false;
    }

    status = psa_hash_update(&innerHash, data, dataLength);
    if (status != PSA_SUCCESS) {
        this->log_WARNING_HI_CryptoComputationError(static_cast<U32>(status));
        return false;
    }

    U8 innerHashOutput[32];
    size_t innerHashLen = 0;
    status = psa_hash_finish(&innerHash, innerHashOutput, sizeof(innerHashOutput), &innerHashLen);
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
    if (status != PSA_SUCCESS) {
        this->log_WARNING_HI_CryptoComputationError(static_cast<U32>(status));
        return false;
    }

    status = psa_hash_update(&outerHash, outerKey, kBlockSize);
    if (status != PSA_SUCCESS) {
        this->log_WARNING_HI_CryptoComputationError(static_cast<U32>(status));
        return false;
    }

    status = psa_hash_update(&outerHash, innerHashOutput, innerHashLen);
    if (status != PSA_SUCCESS) {
        this->log_WARNING_HI_CryptoComputationError(static_cast<U32>(status));
        return false;
    }

    U8 macOutput[32];
    size_t macOutputLength = 0;
    status = psa_hash_finish(&outerHash, macOutput, sizeof(macOutput), &macOutputLength);
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
    // TODO(nateinaction): Check param validity and handle case where param is invalid
    /*
     * Compute the difference between received and expected sequence numbers using unsigned
     * 32-bit arithmetic. This handles wraparound correctly due to the well-defined behavior
     * of unsigned integer overflow in C++. For example, if expected=0xFFFFFFFE and received=1,
     * then (received - expected) == 3 (modulo 2^32). This is a standard technique for
     * sequence number window validation (see RFC 1982: Serial Number Arithmetic).
     */
    // TODO(nateinaction): This wraparound behavior is a good candidate for unit testing
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

}  // namespace Components

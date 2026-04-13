// ======================================================================
// \title  PacketParser.cpp
// \brief  cpp file for PacketParser implementation class
// ======================================================================

#include "PacketParser.hpp"

#include <cstring>

namespace Components {

// ----------------------------------------------------------------------
// Component construction and destruction
// ----------------------------------------------------------------------

PacketParser ::PacketParser() {}

PacketParser ::~PacketParser() {}

// ----------------------------------------------------------------------
// Public helper methods
// ----------------------------------------------------------------------

bool PacketParser ::parsePacket(const uint8_t* dataBuffer,
                                const size_t dataSize,
                                const uint32_t expectedSequenceNumber,
                                const uint32_t sequenceNumberWindow) const {
    // Parse sequence number
    uint32_t sequenceNumber;
    if (!this->parseSequenceNumber(dataBuffer, dataSize, sequenceNumber)) {
        // TODO(nateinaction): Handle failure, maybe pass back custom error?
        return false;
    }

    // TODO(nateinaction): Extract sequence number check to new method
    // If sequence number is invalid, no need to parse further
    const uint32_t delta = sequenceNumber - expectedSequenceNumber;
    if (delta > sequenceNumberWindow) {
        // TODO(nateinaction): Handle failure, maybe pass back custom error?
        return false;
    }

    // Parse opcode
    uint32_t opCode;
    if (!this->parseOpCode(dataBuffer, dataSize, opCode)) {
        // TODO(nateinaction): Handle failure, maybe pass back custom error?
        return false;
    }

    // TODO(nateinaction): Extract opcode check to new method
    // If opcode allows bypass, no need to parse hmac
    static constexpr uint32_t kBypassOpCodes[] = {
        0x01000000,  // no op
        0x2200B000,  // get sequence number
        0x10065000,  // amateurRadio.TELL_JOKE
    };
    constexpr size_t kBypassOpCodesArrayLength = sizeof(kBypassOpCodes) / sizeof(kBypassOpCodes[0]);

    for (size_t i = 0; i < kBypassOpCodesArrayLength; i++) {
        if (opCode == kBypassOpCodes[i]) {
            return true;
        }
    }

    // Parse hmac
    std::array<std::uint8_t, kHmacLength> hmacTrailer;
    if (!this->parseHmac(dataBuffer, dataSize, hmacTrailer)) {
        // TODO(nateinaction): Handle failure, maybe pass back custom error?
        return false;
    }

    // compute hmac

    // compare hmac

    return true;
}

// ----------------------------------------------------------------------
//  Private helper methods
// ----------------------------------------------------------------------

bool PacketParser ::parseSequenceNumber(const uint8_t* dataBuffer,
                                        const size_t dataSize,
                                        uint32_t& sequenceNumber) const {
    // Validate header size
    if (!dataBuffer || dataSize < kHeaderLength)
        return false;

    // Extract security header
    std::array<std::uint8_t, PacketParser::kHeaderLength> header;
    std::memcpy(header.data(), dataBuffer, header.size());

    // Extract sequence number from header bytes 2-5
    sequenceNumber = (static_cast<uint32_t>(header[2]) << 24) | (static_cast<uint32_t>(header[3]) << 16) |
                     (static_cast<uint32_t>(header[4]) << 8) | static_cast<uint32_t>(header[5]);

    return true;
}

bool PacketParser ::parseOpCode(const uint8_t* dataBuffer, const size_t dataLength, uint32_t& opCode) const {
    constexpr const size_t kOpCodeStart = 2;
    constexpr const size_t kOpCodeLength = 4;

    // Validate buffer size
    if (!dataBuffer || dataLength < kHeaderLength + kOpCodeStart + kOpCodeLength)
        return false;

    // Extract opcode bytes
    const uint8_t* opCodePtr = dataBuffer + kHeaderLength + kOpCodeStart;
    opCode = (static_cast<uint32_t>(opCodePtr[0]) << 24) | (static_cast<uint32_t>(opCodePtr[1]) << 16) |
             (static_cast<uint32_t>(opCodePtr[2]) << 8) | static_cast<uint32_t>(opCodePtr[3]);

    return true;
}

bool PacketParser ::parseHmac(const uint8_t* dataBuffer,
                              const size_t dataSize,
                              std::array<std::uint8_t, PacketParser::kHmacLength>& hmac) const {
    // Validate buffer size
    if (!dataBuffer || dataSize < kHmacLength)
        return false;

    // Extract security trailer
    std::memcpy(hmac.data(), dataBuffer + dataSize - hmac.size(), hmac.size());

    return true;
}

bool PacketParser ::computeHmac(const uint8_t* dataBuffer,
                                const size_t dataSize,
                                std::array<std::uint8_t, kHmacLength>& hmac) const {
    // TODO(nateinaction): Can this be replaced with psa_mac_compute

    // constexpr size_t kHmacOutputLength = 16;  // CCSDS 355.0-B-2 specifies 16 bytes

    // // TODO(nateinaction): hmmm outputSize < kHmacOutputLength compares two constants both of size 16
    // if (output == nullptr || outputSize < kHmacOutputLength) {
    //     return false;
    // }

    // // Initialize PSA crypto (idempotent, safe to call multiple times)
    // psa_status_t status = psa_crypto_init();
    // if (status != PSA_SUCCESS) {
    //     this->log_WARNING_HI_CryptoComputationError(static_cast<U32>(status));
    //     return false;
    // }

    // // Parse key from hex string (32 hex characters = 16 bytes)
    // const char* keyStr = key.toChar();
    // if (key.length() != 32) {
    //     this->log_WARNING_HI_InvalidSPI(-1);
    //     return false;
    // }

    // // Parse hex string to bytes
    // constexpr size_t kKeySize = 16;
    // U8 keyBytes[kKeySize];
    // for (FwSizeType i = 0; i < 32; i += 2) {
    //     char byteStr[3] = {keyStr[i], keyStr[i + 1], '\0'};
    //     keyBytes[i / 2] = static_cast<U8>(std::strtoul(byteStr, nullptr, 16));
    // }

    // // Implement HMAC-SHA-256 using PSA hash API (RFC 2104)
    // // HMAC(k, m) = H(k XOR opad || H(k XOR ipad || m))
    // constexpr size_t kBlockSize = 64;  // SHA-256 block size
    // constexpr U8 kIpad = 0x36;
    // constexpr U8 kOpad = 0x5C;

    // // Prepare key: pad with zeros (key is always 16 bytes, which is < 64)
    // U8 preparedKey[kBlockSize] = {0};
    // std::memcpy(preparedKey, keyBytes, kKeySize);

    // // Compute inner hash: H(k XOR ipad || m)
    // U8 innerKey[kBlockSize];
    // for (size_t i = 0; i < kBlockSize; i++) {
    //     innerKey[i] = preparedKey[i] ^ kIpad;
    // }

    // psa_hash_operation_t innerHash = PSA_HASH_OPERATION_INIT;
    // status = psa_hash_setup(&innerHash, PSA_ALG_SHA_256);
    // if (status != PSA_SUCCESS) {
    //     this->log_WARNING_HI_CryptoComputationError(static_cast<U32>(status));
    //     return false;
    // }

    // status = psa_hash_update(&innerHash, innerKey, kBlockSize);
    // if (status != PSA_SUCCESS) {
    //     this->log_WARNING_HI_CryptoComputationError(static_cast<U32>(status));
    //     return false;
    // }

    // status = psa_hash_update(&innerHash, data, dataLength);
    // if (status != PSA_SUCCESS) {
    //     this->log_WARNING_HI_CryptoComputationError(static_cast<U32>(status));
    //     return false;
    // }

    // U8 innerHashOutput[32];
    // size_t innerHashLen = 0;
    // status = psa_hash_finish(&innerHash, innerHashOutput, sizeof(innerHashOutput), &innerHashLen);
    // if (status != PSA_SUCCESS) {
    //     this->log_WARNING_HI_CryptoComputationError(static_cast<U32>(status));
    //     return false;
    // }

    // // Compute outer hash: H(k XOR opad || innerHash)
    // U8 outerKey[kBlockSize];
    // for (size_t i = 0; i < kBlockSize; i++) {
    //     outerKey[i] = preparedKey[i] ^ kOpad;
    // }

    // psa_hash_operation_t outerHash = PSA_HASH_OPERATION_INIT;
    // status = psa_hash_setup(&outerHash, PSA_ALG_SHA_256);
    // if (status != PSA_SUCCESS) {
    //     this->log_WARNING_HI_CryptoComputationError(static_cast<U32>(status));
    //     return false;
    // }

    // status = psa_hash_update(&outerHash, outerKey, kBlockSize);
    // if (status != PSA_SUCCESS) {
    //     this->log_WARNING_HI_CryptoComputationError(static_cast<U32>(status));
    //     return false;
    // }

    // status = psa_hash_update(&outerHash, innerHashOutput, innerHashLen);
    // if (status != PSA_SUCCESS) {
    //     this->log_WARNING_HI_CryptoComputationError(static_cast<U32>(status));
    //     return false;
    // }

    // U8 macOutput[32];
    // size_t macOutputLength = 0;
    // status = psa_hash_finish(&outerHash, macOutput, sizeof(macOutput), &macOutputLength);
    // if (status != PSA_SUCCESS) {
    //     this->log_WARNING_HI_CryptoComputationError(static_cast<U32>(status));
    //     return false;
    // }

    // // Copy first 16 bytes to output buffer
    // std::memcpy(output, macOutput, kHmacOutputLength);
    // return true;
}

bool PacketParser ::validateSequenceNumber(uint32_t expected, uint32_t actual, uint32_t window) const {
    /*
     * Compute the difference between received and expected sequence numbers using unsigned
     * 32-bit arithmetic. This handles wraparound correctly due to the well-defined behavior
     * of unsigned integer overflow in C++. For example, if expected=0xFFFFFFFE and received=1,
     * then (received - expected) == 3 (modulo 2^32). This is a standard technique for
     * sequence number window validation (see RFC 1982: Serial Number Arithmetic).
     */
    // TODO(nateinaction): This wraparound behavior is a good candidate for unit testing
    const uint32_t delta = actual - expected;
    return delta <= window;
}

bool PacketParser ::validateOpCodeBypassAllowed(uint32_t opCode) const {
    static constexpr uint32_t kBypassOpCodes[] = {
        0x01000000,  // no op
        0x2200B000,  // get sequence number
        0x10065000,  // amateurRadio.TELL_JOKE
    };
    constexpr size_t kBypassOpCodesArrayLength = sizeof(kBypassOpCodes) / sizeof(kBypassOpCodes[0]);

    for (size_t i = 0; i < kBypassOpCodesArrayLength; i++) {
        if (opCode == kBypassOpCodes[i]) {
            return true;
        }
    }

    return false;
}

bool PacketParser ::validateHmac(const std::array<std::uint8_t, kHmacLength>& expected,
                                 const std::array<std::uint8_t, kHmacLength>& actual) const {
    // TODO(nateinaction): Use psa_mac_verify for constant time comparison to prevent timing attacks
    return expected == actual;
}

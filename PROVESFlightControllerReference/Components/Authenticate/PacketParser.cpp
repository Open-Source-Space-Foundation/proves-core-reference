// ======================================================================
// \title  PacketParser.cpp
// \brief  cpp file for PacketParser implementation class
// ======================================================================

#include "PacketParser.hpp"

#include <psa/crypto.h>

#include <cstring>

// Include generated header with default key (generated at build time)
#include "AuthDefaultKey.h"

namespace Components {

constexpr size_t kHmacOutputLength = 16;  // CCSDS 355.0-B-2 specifies 16 bytes
constexpr size_t kSha256OutputLength = 32;
constexpr size_t kKeyHexLength = 32;
constexpr size_t kKeySize = 16;

bool hexToNibble(char ch, uint8_t& nibble) {
    if (ch >= '0' && ch <= '9') {
        nibble = static_cast<uint8_t>(ch - '0');
        return true;
    }
    if (ch >= 'a' && ch <= 'f') {
        nibble = static_cast<uint8_t>(10 + (ch - 'a'));
        return true;
    }
    if (ch >= 'A' && ch <= 'F') {
        nibble = static_cast<uint8_t>(10 + (ch - 'A'));
        return true;
    }
    return false;
}

bool parseHexKey(const char* key, uint8_t (&keyBytes)[kKeySize]) {
    if (key == nullptr) {
        return false;
    }

    const char* keyStr = key;
    if (std::strlen(keyStr) != kKeyHexLength) {
        return false;
    }

    for (size_t i = 0; i < kKeyHexLength; i += 2) {
        uint8_t upper = 0;
        uint8_t lower = 0;
        if (!hexToNibble(keyStr[i], upper) || !hexToNibble(keyStr[i + 1], lower)) {
            return false;
        }
        keyBytes[i / 2] = static_cast<uint8_t>((upper << 4) | lower);
    }

    return true;
}

psa_status_t importHmacKey(const uint8_t (&keyBytes)[kKeySize], psa_key_usage_t usage, psa_key_id_t& keyId) {
    psa_key_attributes_t attributes = PSA_KEY_ATTRIBUTES_INIT;
    psa_set_key_type(&attributes, PSA_KEY_TYPE_HMAC);
    psa_set_key_bits(&attributes, static_cast<size_t>(kKeySize * 8));
    psa_set_key_usage_flags(&attributes, usage);
    psa_set_key_algorithm(&attributes, PSA_ALG_HMAC(PSA_ALG_SHA_256));

    const psa_status_t status = psa_import_key(&attributes, keyBytes, sizeof(keyBytes), &keyId);
    psa_reset_key_attributes(&attributes);
    return status;
}

// ----------------------------------------------------------------------
// Component construction and destruction
// ----------------------------------------------------------------------

PacketParser ::PacketParser() {}

PacketParser ::~PacketParser() {}

// ----------------------------------------------------------------------
// Public helper methods
// ----------------------------------------------------------------------

PacketParser::ParseResult PacketParser ::parsePacket(const uint8_t* dataBuffer,
                                                     const size_t dataSize,
                                                     const uint32_t expectedSequenceNumber,
                                                     const uint32_t sequenceNumberWindow) const {
    // Parse SPI
    uint32_t spi;
    if (!this->parseSpi(dataBuffer, dataSize, spi)) {
        return ParseResult::SpiParseError;
    }

    // If SPI is invalid, no need to parse further
    if (!this->validateSpi(spi)) {
        return ParseResult::SpiValidationError;
    }

    // Parse sequence number
    uint32_t sequenceNumber;
    if (!this->parseSequenceNumber(dataBuffer, dataSize, sequenceNumber)) {
        return ParseResult::SequenceNumberParseError;
    }

    // If sequence number is invalid, no need to parse further
    if (!this->validateSequenceNumber(expectedSequenceNumber, sequenceNumber, sequenceNumberWindow)) {
        return ParseResult::SequenceNumberValidationError;
    }

    // Parse opcode
    uint32_t opCode;
    if (!this->parseOpCode(dataBuffer, dataSize, opCode)) {
        return ParseResult::OpCodeParseError;
    }

    // If opcode allows bypass, no need to parse hmac
    if (this->validateOpCodeBypassAllowed(opCode)) {
        return ParseResult::Bypass;
    }

    // Parse hmac
    std::array<uint8_t, kHmacLength> hmacTrailer;
    if (!this->parseHmac(dataBuffer, dataSize, hmacTrailer)) {
        return ParseResult::HmacParseError;
    }

    // if hmac is invalid, reject packet
    if (!this->validateHmac(dataBuffer, dataSize, hmacTrailer)) {
        return ParseResult::HmacValidationError;
    }

    return ParseResult::Ok;
}

// ----------------------------------------------------------------------
//  Private helper methods
// ----------------------------------------------------------------------

bool PacketParser ::parseSpi(const uint8_t* dataBuffer, const size_t dataSize, uint32_t& spi) const {
    // Validate header size
    if (!dataBuffer || dataSize < kHeaderLength)
        return false;

    // Extract SPI from header bytes 0-1
    spi = (static_cast<uint32_t>(dataBuffer[0]) << 8) | static_cast<uint32_t>(dataBuffer[1]);

    return true;
}

bool PacketParser ::parseSequenceNumber(const uint8_t* dataBuffer,
                                        const size_t dataSize,
                                        uint32_t& sequenceNumber) const {
    // Validate header size
    if (!dataBuffer || dataSize < kHeaderLength)
        return false;

    // Extract security header
    std::array<uint8_t, PacketParser::kHeaderLength> header;
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
                              std::array<uint8_t, PacketParser::kHmacLength>& hmac) const {
    // Validate buffer size
    if (!dataBuffer || dataSize < kHmacLength)
        return false;

    // Extract security trailer
    std::memcpy(hmac.data(), dataBuffer + dataSize - hmac.size(), hmac.size());

    return true;
}

bool PacketParser ::validateSpi(uint32_t spi) const {
    // For now we only support SPI 0, which indicates no additional security processing beyond HMAC
    return spi == 0;
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

bool PacketParser ::validateHmac(const uint8_t* dataBuffer,
                                 const size_t dataSize,
                                 const std::array<uint8_t, kHmacLength>& expectedHmac) const {
    psa_status_t status = psa_crypto_init();
    if (status != PSA_SUCCESS) {
        // TODO(nateinaction): Handle failure, maybe pass back custom error?
        return false;
    }

    uint8_t keyBytes[kKeySize];
    if (!parseHexKey(AUTH_DEFAULT_KEY, keyBytes)) {
        // TODO(nateinaction): Handle failure, maybe pass back custom error?
        return false;
    }

    psa_key_id_t keyId = 0;
    status = importHmacKey(keyBytes, PSA_KEY_USAGE_VERIFY_MESSAGE, keyId);
    if (status != PSA_SUCCESS) {
        // TODO(nateinaction): Handle failure, maybe pass back custom error?
        return false;
    }

    uint8_t macOutput[kSha256OutputLength];
    size_t macOutputLength = 0;
    status = psa_mac_verify(keyId, PSA_ALG_HMAC(PSA_ALG_SHA_256),
                            dataBuffer - kHmacLength,  // Authenticate over header + body
                            dataSize - kHmacLength, macOutput, macOutputLength);
    psa_status_t destroyStatus = psa_destroy_key(keyId);
    if (destroyStatus != PSA_SUCCESS) {
        // TODO(nateinaction): Handle failure, maybe pass back custom error?
        return false;
    }

    if (status == PSA_ERROR_INVALID_SIGNATURE) {
        // TODO(nateinaction): Handle failure, maybe pass back custom error?
        return false;
    }
    if (status != PSA_SUCCESS) {
        // TODO(nateinaction): Handle failure, maybe pass back custom error?
        return false;
    }

    return true;
}

}  // namespace Components

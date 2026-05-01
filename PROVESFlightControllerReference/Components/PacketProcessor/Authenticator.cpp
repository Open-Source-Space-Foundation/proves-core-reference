// ======================================================================
// \title  Authenticator.cpp
// \brief  cpp file for packet HMAC authentication functions
// ======================================================================

#include "Authenticator.hpp"

#include <psa/crypto.h>

#include <cstring>

// Include generated header with default key (generated at build time)
#include "AuthDefaultKey.h"

namespace Components {
namespace {

constexpr size_t kKeyHexLength = 32;
constexpr size_t kSha256BlockSize = 64;
constexpr size_t kSha256OutputSize = 32;

// Convert a single hex character to its numeric nibble value.
// Returns true and sets `nibble` on success; returns false on invalid input.
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

// Parse a 32-character hex string (16 bytes) into a byte array.
// Returns true on success and fills `keyBytes` with the parsed bytes.
bool parseHexKey(const char* key, uint8_t (&keyBytes)[Ccsds355_0_B_2_Cmac::kSecurityTrailerSize]) {
    if (key == nullptr) {
        return false;
    }

    if (std::strlen(key) != kKeyHexLength) {
        return false;
    }

    for (size_t i = 0; i < kKeyHexLength; i += 2) {
        uint8_t upper = 0;
        uint8_t lower = 0;
        if (!hexToNibble(key[i], upper) || !hexToNibble(key[i + 1], lower)) {
            return false;
        }
        keyBytes[i / 2] = static_cast<uint8_t>((upper << 4) | lower);
    }

    return true;
}

// Compute an HMAC-SHA256 over `dataBuffer` using `keyBytes`, then truncate
// the result to the CCSDS security trailer size. Uses the PSA hash API to
// perform the HMAC according to RFC2104 using hash primitives.
// Returns a PSA status code and writes the truncated MAC to `hmacOut`.
psa_status_t computeHmac(const uint8_t* dataBuffer,
                         size_t dataSize,
                         const uint8_t (&keyBytes)[Ccsds355_0_B_2_Cmac::kSecurityTrailerSize],
                         uint8_t (&hmacOut)[Ccsds355_0_B_2_Cmac::kSecurityTrailerSize]) {
    if (dataBuffer == nullptr) {
        return PSA_ERROR_INVALID_ARGUMENT;
    }

    // Prepare the key block (left-justify key bytes and zero-pad to block size)
    uint8_t preparedKey[kSha256BlockSize] = {0};
    std::memcpy(preparedKey, keyBytes, sizeof(keyBytes));

    // Build inner and outer pads for HMAC (ipad = 0x36, opad = 0x5c)
    uint8_t innerPad[kSha256BlockSize];
    uint8_t outerPad[kSha256BlockSize];
    for (size_t i = 0; i < kSha256BlockSize; i++) {
        innerPad[i] = static_cast<uint8_t>(preparedKey[i] ^ 0x36U);
        outerPad[i] = static_cast<uint8_t>(preparedKey[i] ^ 0x5CU);
    }

    psa_hash_operation_t innerHash = PSA_HASH_OPERATION_INIT;
    psa_status_t status = psa_hash_setup(&innerHash, PSA_ALG_SHA_256);
    if (status != PSA_SUCCESS) {
        return status;
    }

    // Hash inner pad then the message: H(innerPad || data)
    status = psa_hash_update(&innerHash, innerPad, sizeof(innerPad));
    if (status == PSA_SUCCESS) {
        status = psa_hash_update(&innerHash, dataBuffer, dataSize);
    }

    uint8_t innerDigest[kSha256OutputSize];
    size_t innerDigestLen = 0;
    if (status == PSA_SUCCESS) {
        status = psa_hash_finish(&innerHash, innerDigest, sizeof(innerDigest), &innerDigestLen);
    }
    if (status != PSA_SUCCESS) {
        psa_hash_abort(&innerHash);
        return status;
    }

    psa_hash_operation_t outerHash = PSA_HASH_OPERATION_INIT;
    status = psa_hash_setup(&outerHash, PSA_ALG_SHA_256);
    if (status != PSA_SUCCESS) {
        return status;
    }

    // Hash outer pad then the inner digest: H(outerPad || innerDigest)
    status = psa_hash_update(&outerHash, outerPad, sizeof(outerPad));
    if (status == PSA_SUCCESS) {
        status = psa_hash_update(&outerHash, innerDigest, innerDigestLen);
    }

    uint8_t fullMac[kSha256OutputSize];
    size_t fullMacLen = 0;
    if (status == PSA_SUCCESS) {
        status = psa_hash_finish(&outerHash, fullMac, sizeof(fullMac), &fullMacLen);
    }
    if (status != PSA_SUCCESS) {
        psa_hash_abort(&outerHash);
        return status;
    }

    if (fullMacLen < Ccsds355_0_B_2_Cmac::kSecurityTrailerSize) {
        return PSA_ERROR_GENERIC_ERROR;
    }

    // Copy the first kSecurityTrailerSize bytes as the truncated MAC
    std::memcpy(hmacOut, fullMac, Ccsds355_0_B_2_Cmac::kSecurityTrailerSize);
    return PSA_SUCCESS;
}

}  // namespace

PacketAuthenticator::Result authenticatePacket(const uint8_t* dataBuffer, size_t dataSize, const Hmac& hmac) {
    // Basic input validation: buffer present and at least trailer-sized
    if (!dataBuffer || dataSize < Ccsds355_0_B_2_Cmac::kSecurityTrailerSize) {
        return PacketAuthenticator::Result{PacketAuthenticator::Status::VerifyError, PSA_ERROR_INVALID_ARGUMENT};
    }

    // Initialize PSA crypto library (idempotent)
    const psa_status_t initStatus = psa_crypto_init();
    if (initStatus != PSA_SUCCESS) {
        return PacketAuthenticator::Result{PacketAuthenticator::Status::InitError, initStatus};
    }

    // Parse the hex-encoded default key into raw bytes
    uint8_t keyBytes[Ccsds355_0_B_2_Cmac::kSecurityTrailerSize];
    if (!parseHexKey(AUTH_DEFAULT_KEY, keyBytes)) {
        return PacketAuthenticator::Result{PacketAuthenticator::Status::ParseKeyError, PSA_ERROR_INVALID_ARGUMENT};
    }

    // Compute the length of the authenticated portion (exclude trailer)
    const size_t authenticatedDataSize = dataSize - Ccsds355_0_B_2_Cmac::kSecurityTrailerSize;

    // Compute truncated HMAC over the authenticated data
    uint8_t computedHmac[Ccsds355_0_B_2_Cmac::kSecurityTrailerSize];
    const psa_status_t status = computeHmac(dataBuffer, authenticatedDataSize, keyBytes, computedHmac);
    if (status != PSA_SUCCESS) {
        return PacketAuthenticator::Result{PacketAuthenticator::Status::VerifyError, status};
    }

    // Compare computed truncated HMAC with provided trailer
    if (std::memcmp(computedHmac, hmac.data(), hmac.size()) != 0) {
        return PacketAuthenticator::Result{PacketAuthenticator::Status::VerifyError, PSA_ERROR_INVALID_SIGNATURE};
    }

    return PacketAuthenticator::Result{PacketAuthenticator::Status::Authenticated, PSA_SUCCESS};
}

}  // namespace Components

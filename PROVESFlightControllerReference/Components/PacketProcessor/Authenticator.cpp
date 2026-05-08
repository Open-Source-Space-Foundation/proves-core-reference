// ======================================================================
// \title  Authenticator.cpp
// \brief  cpp file for packet HMAC authentication functions
// ======================================================================

#include "Authenticator.hpp"

#include <mbedtls/platform_util.h>
#include <psa/crypto.h>

#include <cstring>

namespace Components {
namespace {

constexpr size_t kKeyHexLength =
    Ccsds355_0_B_2_Cmac::kSecurityTrailerSize *
    2;  //!< The expected length of the hex-encoded key string (16 bytes * 2 characters/byte)

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

// Import an HMAC key into PSA for message verification.
psa_status_t importHmacKey(const uint8_t (&keyBytes)[Ccsds355_0_B_2_Cmac::kSecurityTrailerSize],
                           psa_key_usage_t usage,
                           psa_key_id_t& keyId) {
    psa_key_attributes_t attributes = PSA_KEY_ATTRIBUTES_INIT;
    psa_set_key_type(&attributes, PSA_KEY_TYPE_HMAC);
    psa_set_key_bits(&attributes, static_cast<size_t>(Ccsds355_0_B_2_Cmac::kSecurityTrailerSize * 8));
    psa_set_key_usage_flags(&attributes, usage);
    psa_set_key_algorithm(&attributes, PSA_ALG_AT_LEAST_THIS_LENGTH_MAC(PSA_ALG_HMAC(PSA_ALG_SHA_256),
                                                                        Ccsds355_0_B_2_Cmac::kSecurityTrailerSize));

    const psa_status_t status = psa_import_key(&attributes, keyBytes, sizeof(keyBytes), &keyId);
    psa_reset_key_attributes(&attributes);
    return status;
}

}  // namespace

PacketAuthenticator::Result authenticatePacket(const uint8_t* dataBuffer,
                                               size_t dataSize,
                                               const Hmac& hmac,
                                               const char* key) {
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
    if (!parseHexKey(key, keyBytes)) {
        return PacketAuthenticator::Result{PacketAuthenticator::Status::ParseKeyError, PSA_ERROR_INVALID_ARGUMENT};
    }

    const size_t authenticatedDataSize = dataSize - Ccsds355_0_B_2_Cmac::kSecurityTrailerSize;

    // Import the key into PSA and verify the HMAC
    psa_key_id_t keyId = 0;
    psa_status_t status = importHmacKey(keyBytes, PSA_KEY_USAGE_VERIFY_MESSAGE, keyId);
    mbedtls_platform_zeroize(keyBytes, sizeof keyBytes);
    if (status != PSA_SUCCESS) {
        return PacketAuthenticator::Result{PacketAuthenticator::Status::ImportKeyError, status};
    }

    // Verify the HMAC on the packet data (excluding the trailer)
    status = psa_mac_verify(
        keyId, PSA_ALG_TRUNCATED_MAC(PSA_ALG_HMAC(PSA_ALG_SHA_256), Ccsds355_0_B_2_Cmac::kSecurityTrailerSize),
        dataBuffer, authenticatedDataSize, hmac.data(), hmac.size());

    // Destroy the key handle to clean up resources
    const psa_status_t destroyStatus = psa_destroy_key(keyId);
    if (destroyStatus != PSA_SUCCESS) {
        return PacketAuthenticator::Result{PacketAuthenticator::Status::DestroyKeyError, destroyStatus};
    }

    // Check the result of the verification step
    if (status != PSA_SUCCESS) {
        return PacketAuthenticator::Result{PacketAuthenticator::Status::VerifyError, status};
    }

    return PacketAuthenticator::Result{PacketAuthenticator::Status::Authenticated, PSA_SUCCESS};
}

}  // namespace Components

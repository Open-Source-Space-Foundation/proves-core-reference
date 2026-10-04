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
    Ccsds355_0_B_2::kTCSecurityTrailer *
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
bool parseHexKey(const char* key, uint8_t (&keyBytes)[Ccsds355_0_B_2::kTCSecurityTrailer]) {
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

}  // namespace

// Import an HMAC key into PSA for message verification.
PacketAuthenticator::KeyImportResult importHmacKey(const char* key, uint32_t& keyId) {
    // Initialize PSA crypto library
    const psa_status_t initStatus = psa_crypto_init();
    if (initStatus != PSA_SUCCESS) {
        return {PacketAuthenticator::KeyImportStatus::InitError, initStatus};
    }

    // Parse the hex-encoded default key into raw bytes
    uint8_t keyBytes[Ccsds355_0_B_2::kTCSecurityTrailer];
    if (!parseHexKey(key, keyBytes)) {
        return {PacketAuthenticator::KeyImportStatus::ParseKeyError, PSA_ERROR_INVALID_ARGUMENT};
    }

    // Set up the key attributes
    psa_key_attributes_t attributes = PSA_KEY_ATTRIBUTES_INIT;
    psa_set_key_type(&attributes, PSA_KEY_TYPE_HMAC);
    psa_set_key_bits(&attributes, static_cast<size_t>(Ccsds355_0_B_2::kTCSecurityTrailer * 8));
    psa_set_key_usage_flags(&attributes, PSA_KEY_USAGE_VERIFY_MESSAGE);
    psa_set_key_algorithm(&attributes, PSA_ALG_AT_LEAST_THIS_LENGTH_MAC(PSA_ALG_HMAC(PSA_ALG_SHA_256),
                                                                        Ccsds355_0_B_2::kTCSecurityTrailer));
    psa_set_key_lifetime(&attributes, PSA_KEY_LIFETIME_VOLATILE);

    // Import the key into PSA key store
    const psa_status_t status = psa_import_key(&attributes, keyBytes, sizeof(keyBytes), &keyId);

    // Clean up sensitive data regardless of import outcome
    psa_reset_key_attributes(&attributes);
    mbedtls_platform_zeroize(keyBytes, sizeof keyBytes);

    if (status != PSA_SUCCESS) {
        return {PacketAuthenticator::KeyImportStatus::ImportKeyError, status};
    }

    return {PacketAuthenticator::KeyImportStatus::Success, PSA_SUCCESS};
}

PacketAuthenticator::AuthenticationResult authenticateFrame(uint16_t saIndex,
                                                            const uint8_t* dataBuffer,
                                                            size_t dataSize,
                                                            const Mac& hmac,
                                                            uint32_t& keyId) {
    // Basic input validation: buffer present and at least trailer-sized
    if (!dataBuffer || dataSize < Ccsds355_0_B_2::kTCSecurityTrailer) {
        return {PacketAuthenticator::AuthenticationStatus::VerifyError, PSA_ERROR_INVALID_ARGUMENT};
    }

    // The MAC covers the SA index (stripped from the buffer by the upstream deframer) followed by
    // the frame data, so it must be verified via the PSA multipart API rather than a single call.
    const size_t authenticatedDataSize = dataSize - Ccsds355_0_B_2::kTCSecurityTrailer;
    const uint8_t saIndexBytes[2] = {static_cast<uint8_t>(saIndex >> 8), static_cast<uint8_t>(saIndex & 0xFF)};

    psa_mac_operation_t operation = PSA_MAC_OPERATION_INIT;
    psa_status_t status = psa_mac_verify_setup(
        &operation, keyId, PSA_ALG_TRUNCATED_MAC(PSA_ALG_HMAC(PSA_ALG_SHA_256), Ccsds355_0_B_2::kTCSecurityTrailer));
    if (status != PSA_SUCCESS) {
        return {PacketAuthenticator::AuthenticationStatus::VerifyError, status};
    }

    status = psa_mac_update(&operation, saIndexBytes, sizeof(saIndexBytes));
    if (status != PSA_SUCCESS) {
        psa_mac_abort(&operation);
        return {PacketAuthenticator::AuthenticationStatus::VerifyError, status};
    }

    status = psa_mac_update(&operation, dataBuffer, authenticatedDataSize);
    if (status != PSA_SUCCESS) {
        psa_mac_abort(&operation);
        return {PacketAuthenticator::AuthenticationStatus::VerifyError, status};
    }

    // psa_mac_verify_finish aborts the operation internally on both success and failure
    status = psa_mac_verify_finish(&operation, hmac.data(), hmac.size());
    if (status != PSA_SUCCESS) {
        return {PacketAuthenticator::AuthenticationStatus::VerifyError, status};
    }

    return {PacketAuthenticator::AuthenticationStatus::Authenticated, PSA_SUCCESS};
}

}  // namespace Components

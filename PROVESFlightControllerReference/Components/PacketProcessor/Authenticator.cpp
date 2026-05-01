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

psa_status_t importHmacKey(const uint8_t (&keyBytes)[Ccsds355_0_B_2_Cmac::kSecurityTrailerSize],
                           psa_key_usage_t usage,
                           psa_key_id_t& keyId) {
    psa_key_attributes_t attributes = PSA_KEY_ATTRIBUTES_INIT;
    psa_set_key_type(&attributes, PSA_KEY_TYPE_HMAC);
    psa_set_key_bits(&attributes, static_cast<size_t>(Ccsds355_0_B_2_Cmac::kSecurityTrailerSize * 8));
    psa_set_key_usage_flags(&attributes, usage);
    psa_set_key_algorithm(&attributes, PSA_ALG_HMAC(PSA_ALG_SHA_256));

    const psa_status_t status = psa_import_key(&attributes, keyBytes, sizeof(keyBytes), &keyId);
    psa_reset_key_attributes(&attributes);
    return status;
}

psa_status_t computeTruncatedHmacWithHashApi(const uint8_t* dataBuffer,
                                             size_t dataSize,
                                             const uint8_t (&keyBytes)[Ccsds355_0_B_2_Cmac::kSecurityTrailerSize],
                                             uint8_t (&hmacOut)[Ccsds355_0_B_2_Cmac::kSecurityTrailerSize]) {
    if (dataBuffer == nullptr) {
        return PSA_ERROR_INVALID_ARGUMENT;
    }

    uint8_t preparedKey[kSha256BlockSize] = {0};
    std::memcpy(preparedKey, keyBytes, sizeof(keyBytes));

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

    std::memcpy(hmacOut, fullMac, Ccsds355_0_B_2_Cmac::kSecurityTrailerSize);
    return PSA_SUCCESS;
}

}  // namespace

PacketAuthenticator::Result authenticatePacket(const uint8_t* dataBuffer, size_t dataSize, const Hmac& hmac) {
    if (!dataBuffer || dataSize < Ccsds355_0_B_2_Cmac::kSecurityTrailerSize) {
        return PacketAuthenticator::Result{PacketAuthenticator::Status::VerifyError, PSA_ERROR_INVALID_ARGUMENT};
    }

    psa_status_t status = psa_crypto_init();
    if (status != PSA_SUCCESS) {
        return PacketAuthenticator::Result{PacketAuthenticator::Status::InitError, status};
    }

    uint8_t keyBytes[Ccsds355_0_B_2_Cmac::kSecurityTrailerSize];
    if (!parseHexKey(AUTH_DEFAULT_KEY, keyBytes)) {
        return PacketAuthenticator::Result{PacketAuthenticator::Status::ParseKeyError, PSA_ERROR_INVALID_ARGUMENT};
    }

    const size_t authenticatedDataSize = dataSize - Ccsds355_0_B_2_Cmac::kSecurityTrailerSize;

    psa_key_id_t keyId = 0;
    status = importHmacKey(keyBytes, PSA_KEY_USAGE_VERIFY_MESSAGE, keyId);
    if (status != PSA_SUCCESS) {
        return PacketAuthenticator::Result{PacketAuthenticator::Status::ImportKeyError, status};
    }

    status = psa_mac_verify(keyId, PSA_ALG_HMAC(PSA_ALG_SHA_256), dataBuffer, authenticatedDataSize, hmac.data(),
                            hmac.size());

    const psa_status_t destroyStatus = psa_destroy_key(keyId);
    if (destroyStatus != PSA_SUCCESS) {
        return PacketAuthenticator::Result{PacketAuthenticator::Status::DestroyKeyError, destroyStatus};
    }

    if (status == PSA_SUCCESS) {
        return PacketAuthenticator::Result{PacketAuthenticator::Status::Authenticated, PSA_SUCCESS};
    }

    // Some PSA backends used on embedded targets do not support psa_mac_verify
    // for HMAC-SHA-256. Fall back to RFC2104 HMAC built from psa_hash_* calls.
    if (status != PSA_ERROR_NOT_SUPPORTED && status != PSA_ERROR_NOT_PERMITTED) {
        return PacketAuthenticator::Result{PacketAuthenticator::Status::VerifyError, status};
    }

    uint8_t computedHmac[Ccsds355_0_B_2_Cmac::kSecurityTrailerSize];
    const psa_status_t fallbackStatus =
        computeTruncatedHmacWithHashApi(dataBuffer, authenticatedDataSize, keyBytes, computedHmac);
    if (fallbackStatus != PSA_SUCCESS) {
        return PacketAuthenticator::Result{PacketAuthenticator::Status::VerifyError, fallbackStatus};
    }

    if (std::memcmp(computedHmac, hmac.data(), hmac.size()) != 0) {
        return PacketAuthenticator::Result{PacketAuthenticator::Status::VerifyError, PSA_ERROR_INVALID_SIGNATURE};
    }

    return PacketAuthenticator::Result{PacketAuthenticator::Status::Authenticated, PSA_SUCCESS};
}

}  // namespace Components

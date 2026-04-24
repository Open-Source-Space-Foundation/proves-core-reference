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

    psa_key_id_t keyId = 0;
    // Import key allowing both verify and sign so we can compute full MAC
    // and compare truncated bytes. Some providers restrict psa_mac_compute
    // to keys with SIGN_MESSAGE usage.
    status =
        importHmacKey(keyBytes, (psa_key_usage_t)(PSA_KEY_USAGE_VERIFY_MESSAGE | PSA_KEY_USAGE_SIGN_MESSAGE), keyId);
    if (status != PSA_SUCCESS) {
        return PacketAuthenticator::Result{PacketAuthenticator::Status::ImportKeyError, status};
    }

    unsigned char fullMac[32];
    size_t macLen = 0;
    status = psa_mac_compute(keyId, PSA_ALG_HMAC(PSA_ALG_SHA_256), dataBuffer,
                             dataSize - Ccsds355_0_B_2_Cmac::kSecurityTrailerSize, fullMac, sizeof(fullMac), &macLen);

    const psa_status_t destroyStatus = psa_destroy_key(keyId);
    if (destroyStatus != PSA_SUCCESS) {
        return PacketAuthenticator::Result{PacketAuthenticator::Status::DestroyKeyError, destroyStatus};
    }

    if (status != PSA_SUCCESS) {
        return PacketAuthenticator::Result{PacketAuthenticator::Status::VerifyError, status};
    }

    if (macLen < hmac.size() || std::memcmp(fullMac, hmac.data(), hmac.size()) != 0) {
        return PacketAuthenticator::Result{PacketAuthenticator::Status::VerifyError, PSA_ERROR_INVALID_SIGNATURE};
    }

    return PacketAuthenticator::Result{PacketAuthenticator::Status::Authenticated, PSA_SUCCESS};
}

}  // namespace Components

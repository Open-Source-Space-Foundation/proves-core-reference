#include <gtest/gtest.h>
#include <psa/crypto.h>

#include "PROVESFlightControllerReference/Components/PacketProcessor/AuthDefaultKey.h"
#include "PROVESFlightControllerReference/Components/PacketProcessor/Authenticator.hpp"

using namespace Components;

static bool parseHexKeyToBytes(const char* key, uint8_t* out, size_t outLen) {
    if (!key)
        return false;
    size_t len = std::strlen(key);
    if (len != 32)
        return false;
    for (size_t i = 0; i < 16; ++i) {
        char hi = key[i * 2];
        char lo = key[i * 2 + 1];
        auto hexval = [](char c) -> int {
            if (c >= '0' && c <= '9')
                return c - '0';
            if (c >= 'a' && c <= 'f')
                return 10 + (c - 'a');
            if (c >= 'A' && c <= 'F')
                return 10 + (c - 'A');
            return -1;
        };
        int h = hexval(hi);
        int l = hexval(lo);
        if (h < 0 || l < 0)
            return false;
        out[i] = static_cast<uint8_t>((h << 4) | l);
    }
    return true;
}

static Hmac computeHmacTruncated(const uint8_t* data, size_t dataLen) {
    Hmac out{};
    uint8_t key[16];
    if (!parseHexKeyToBytes(AUTH_DEFAULT_KEY, key, sizeof(key)))
        return out;

    psa_status_t status = psa_crypto_init();
    if (status != PSA_SUCCESS)
        return out;

    psa_key_attributes_t attributes = PSA_KEY_ATTRIBUTES_INIT;
    psa_set_key_type(&attributes, PSA_KEY_TYPE_HMAC);
    psa_set_key_bits(&attributes, static_cast<size_t>(sizeof(key) * 8));
    psa_set_key_usage_flags(&attributes, PSA_KEY_USAGE_SIGN_MESSAGE);
    psa_set_key_algorithm(&attributes, PSA_ALG_HMAC(PSA_ALG_SHA_256));

    psa_key_id_t keyId = 0;
    status = psa_import_key(&attributes, key, sizeof(key), &keyId);
    psa_reset_key_attributes(&attributes);
    if (status != PSA_SUCCESS)
        return out;

    unsigned char fullMac[32];
    size_t outLen = 0;
    status = psa_mac_compute(keyId, PSA_ALG_HMAC(PSA_ALG_SHA_256), data, dataLen, fullMac, sizeof(fullMac), &outLen);

    psa_destroy_key(keyId);
    if (status != PSA_SUCCESS)
        return out;

    // Truncate to security trailer size (16)
    for (size_t i = 0; i < out.size() && i < outLen; ++i)
        out[i] = fullMac[i];
    return out;
}

TEST(PacketAuthenticatorTest, NullBuffer) {
    Hmac hmac{};
    auto res = Components::authenticatePacket(nullptr, 0, hmac);
    EXPECT_EQ(res.status, PacketAuthenticator::Status::VerifyError);
    EXPECT_EQ(res.psaStatus, PSA_ERROR_INVALID_ARGUMENT);
}

TEST(PacketAuthenticatorTest, AuthenticatedSuccess) {
    const size_t payloadLen = 16;
    const size_t totalLen = payloadLen + Ccsds355_0_B_2_Cmac::kSecurityTrailerSize;
    std::vector<uint8_t> buf(totalLen, 0);

    // compute truncated HMAC over payload (buf.data(), payloadLen)
    Hmac hmac = computeHmacTruncated(buf.data(), payloadLen);

    auto res = Components::authenticatePacket(buf.data(), totalLen, hmac);
    EXPECT_EQ(res.status, PacketAuthenticator::Status::Authenticated);
    EXPECT_EQ(res.psaStatus, PSA_SUCCESS);
}

TEST(PacketAuthenticatorTest, VerifyFailure) {
    const size_t payloadLen = 16;
    const size_t totalLen = payloadLen + Ccsds355_0_B_2_Cmac::kSecurityTrailerSize;
    std::vector<uint8_t> buf(totalLen, 0);

    Hmac hmac = computeHmacTruncated(buf.data(), payloadLen);
    // Corrupt one byte
    hmac[0] ^= 0xFF;

    auto res = Components::authenticatePacket(buf.data(), totalLen, hmac);
    EXPECT_EQ(res.status, PacketAuthenticator::Status::VerifyError);
    EXPECT_NE(res.psaStatus, PSA_SUCCESS);
}

#include <gtest/gtest.h>
#include <psa/crypto.h>

#include <vector>

#include "PROVESFlightControllerReference/Components/TcSecurityDeframer/Authenticator.hpp"

using namespace Components;

constexpr char kTestKeyHex[] =
    "14408c2711281f4d70452ce3730bb4fa";  //!< The hex-encoded key corresponding to the MAC in the test packets

//! 16 data bytes followed by their HMAC-SHA-256 MAC truncated to 16 bytes, computed with kTestKeyHex
static const std::vector<uint8_t> kTestPacket = {1,    2,    3,    4,    5,    6,    7,    8,    9,    10,   11,
                                                 12,   13,   14,   15,   16,   0x54, 0x92, 0x46, 0xAF, 0xF2, 0xEA,
                                                 0x86, 0x7C, 0xEB, 0xBC, 0x38, 0x5D, 0x73, 0xF8, 0x94, 0x9C};

//! Import the test key, asserting success, and return the PSA key id
static uint32_t importTestKey() {
    uint32_t keyId = 0;
    auto res = importHmacKey(kTestKeyHex, keyId);
    EXPECT_EQ(res.status, PacketAuthenticator::KeyImportStatus::Success);
    EXPECT_EQ(res.psaStatus, PSA_SUCCESS);
    return keyId;
}

//! Extract the trailing MAC from a packet
static Mac macOf(const std::vector<uint8_t>& packet) {
    Mac mac{};
    std::copy(packet.end() - static_cast<long>(mac.size()), packet.end(), mac.begin());
    return mac;
}

TEST(PacketAuthenticatorTest, ImportInvalidHexKey) {
    uint32_t keyId = 0;
    auto res = importHmacKey("invalidkey", keyId);
    EXPECT_EQ(res.status, PacketAuthenticator::KeyImportStatus::ParseKeyError);
    EXPECT_EQ(res.psaStatus, PSA_ERROR_INVALID_ARGUMENT);
}

TEST(PacketAuthenticatorTest, ImportNullKey) {
    uint32_t keyId = 0;
    auto res = importHmacKey(nullptr, keyId);
    EXPECT_EQ(res.status, PacketAuthenticator::KeyImportStatus::ParseKeyError);
    EXPECT_EQ(res.psaStatus, PSA_ERROR_INVALID_ARGUMENT);
}

TEST(PacketAuthenticatorTest, NullBuffer) {
    uint32_t keyId = importTestKey();
    Mac mac{};
    auto res = authenticatePacket(nullptr, 0, mac, keyId);
    EXPECT_EQ(res.status, PacketAuthenticator::AuthenticationStatus::VerifyError);
    EXPECT_EQ(res.psaStatus, PSA_ERROR_INVALID_ARGUMENT);
}

TEST(PacketAuthenticatorTest, AuthenticatedSuccess) {
    uint32_t keyId = importTestKey();
    auto res = authenticatePacket(kTestPacket.data(), kTestPacket.size(), macOf(kTestPacket), keyId);
    EXPECT_EQ(res.status, PacketAuthenticator::AuthenticationStatus::Authenticated);
    EXPECT_EQ(res.psaStatus, PSA_SUCCESS);
}

TEST(PacketAuthenticatorTest, VerifyFailure) {
    uint32_t keyId = importTestKey();
    Mac mac = macOf(kTestPacket);

    // Corrupt one byte of the MAC
    mac[0] ^= 0xFF;

    auto res = authenticatePacket(kTestPacket.data(), kTestPacket.size(), mac, keyId);
    EXPECT_EQ(res.status, PacketAuthenticator::AuthenticationStatus::VerifyError);
    EXPECT_NE(res.psaStatus, PSA_SUCCESS);
}

TEST(PacketAuthenticatorTest, CorruptedDataFails) {
    uint32_t keyId = importTestKey();
    std::vector<uint8_t> packet = kTestPacket;

    // Corrupt one authenticated data byte
    packet[0] ^= 0xFF;

    auto res = authenticatePacket(packet.data(), packet.size(), macOf(packet), keyId);
    EXPECT_EQ(res.status, PacketAuthenticator::AuthenticationStatus::VerifyError);
    EXPECT_NE(res.psaStatus, PSA_SUCCESS);
}

TEST(PacketAuthenticatorTest, ShortBuffer) {
    uint32_t keyId = importTestKey();
    std::vector<uint8_t> packet = {1, 2, 3};  // Too short to contain a MAC
    Mac mac{};

    auto res = authenticatePacket(packet.data(), packet.size(), mac, keyId);
    EXPECT_EQ(res.status, PacketAuthenticator::AuthenticationStatus::VerifyError);
    EXPECT_EQ(res.psaStatus, PSA_ERROR_INVALID_ARGUMENT);
}

TEST(PacketAuthenticatorTest, MinimumSizeBuffer) {
    uint32_t keyId = importTestKey();

    // Buffer that is exactly the size of the security trailer: zero authenticated bytes
    std::vector<uint8_t> packet(Ccsds355_0_B_2::kTCSecurityTrailer, 0);
    Mac mac{};

    auto res = authenticatePacket(packet.data(), packet.size(), mac, keyId);
    EXPECT_EQ(res.status, PacketAuthenticator::AuthenticationStatus::VerifyError);
    EXPECT_EQ(res.psaStatus, PSA_ERROR_INVALID_SIGNATURE);
}

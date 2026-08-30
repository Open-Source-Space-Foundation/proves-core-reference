#include <gtest/gtest.h>
#include <psa/crypto.h>

#include <vector>

#include "PROVESFlightControllerReference/Components/TcSecurityDecryptor/Authenticator.hpp"

using namespace Components;

constexpr char kTestKeyHex[] =
    "14408c2711281f4d70452ce3730bb4fa";  //!< The hex-encoded key corresponding to the MAC in the test packets

//! 16 data bytes followed by their HMAC-SHA-256 MAC (truncated to 16 bytes) computed with kTestKeyHex
//! over [SA index 0 (2 bytes, big-endian) | data]
static const std::vector<uint8_t> kTestPacket = {1,    2,    3,    4,    5,    6,    7,    8,    9,    10,   11,
                                                 12,   13,   14,   15,   16,   0x8C, 0xA5, 0x91, 0x0D, 0xB6, 0xE0,
                                                 0xC1, 0xEA, 0x6F, 0x06, 0x05, 0x0C, 0x37, 0x89, 0xBC, 0xD9};

//! The same 16 data bytes' HMAC-SHA-256 MAC (truncated to 16 bytes) computed under SA index 1
static const Mac kTestMacUnderSa1 = {0x84, 0xE9, 0xF4, 0xCB, 0xC4, 0xFD, 0x37, 0x5B,
                                     0x66, 0xD3, 0x95, 0x7F, 0xE8, 0xD6, 0xD2, 0x7B};

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
    auto res = authenticateFrame(0, nullptr, 0, mac, keyId);
    EXPECT_EQ(res.status, PacketAuthenticator::AuthenticationStatus::VerifyError);
    EXPECT_EQ(res.psaStatus, PSA_ERROR_INVALID_ARGUMENT);
}

TEST(PacketAuthenticatorTest, AuthenticatedSuccess) {
    uint32_t keyId = importTestKey();
    auto res = authenticateFrame(0, kTestPacket.data(), kTestPacket.size(), macOf(kTestPacket), keyId);
    EXPECT_EQ(res.status, PacketAuthenticator::AuthenticationStatus::Authenticated);
    EXPECT_EQ(res.psaStatus, PSA_SUCCESS);
}

TEST(PacketAuthenticatorTest, SaIndexParticipatesInMac) {
    // The same [data|MAC] bytes verify under SA 0 but fail under SA 1, and the MAC computed
    // for SA 1 fails under SA 0: the SA index is not just a routing key, it is authenticated.
    uint32_t keyId = importTestKey();

    auto resUnderSa1 = authenticateFrame(1, kTestPacket.data(), kTestPacket.size(), macOf(kTestPacket), keyId);
    EXPECT_EQ(resUnderSa1.status, PacketAuthenticator::AuthenticationStatus::VerifyError);

    // authenticateFrame authenticates size-Mac::size() bytes of the buffer, so the buffer must
    // still be full-length (data + trailer space) even though the trailing bytes are unused.
    std::vector<uint8_t> data = kTestPacket;
    auto resSa1MacUnderSa1 = authenticateFrame(1, data.data(), data.size(), kTestMacUnderSa1, keyId);
    EXPECT_EQ(resSa1MacUnderSa1.status, PacketAuthenticator::AuthenticationStatus::Authenticated);

    auto resSa1MacUnderSa0 = authenticateFrame(0, data.data(), data.size(), kTestMacUnderSa1, keyId);
    EXPECT_EQ(resSa1MacUnderSa0.status, PacketAuthenticator::AuthenticationStatus::VerifyError);
}

TEST(PacketAuthenticatorTest, VerifyFailure) {
    uint32_t keyId = importTestKey();
    Mac mac = macOf(kTestPacket);

    // Corrupt one byte of the MAC
    mac[0] ^= 0xFF;

    auto res = authenticateFrame(0, kTestPacket.data(), kTestPacket.size(), mac, keyId);
    EXPECT_EQ(res.status, PacketAuthenticator::AuthenticationStatus::VerifyError);
    EXPECT_NE(res.psaStatus, PSA_SUCCESS);
}

TEST(PacketAuthenticatorTest, CorruptedDataFails) {
    uint32_t keyId = importTestKey();
    std::vector<uint8_t> packet = kTestPacket;

    // Corrupt one authenticated data byte
    packet[0] ^= 0xFF;

    auto res = authenticateFrame(0, packet.data(), packet.size(), macOf(packet), keyId);
    EXPECT_EQ(res.status, PacketAuthenticator::AuthenticationStatus::VerifyError);
    EXPECT_NE(res.psaStatus, PSA_SUCCESS);
}

TEST(PacketAuthenticatorTest, ShortBuffer) {
    uint32_t keyId = importTestKey();
    std::vector<uint8_t> packet = {1, 2, 3};  // Too short to contain a MAC
    Mac mac{};

    auto res = authenticateFrame(0, packet.data(), packet.size(), mac, keyId);
    EXPECT_EQ(res.status, PacketAuthenticator::AuthenticationStatus::VerifyError);
    EXPECT_EQ(res.psaStatus, PSA_ERROR_INVALID_ARGUMENT);
}

TEST(PacketAuthenticatorTest, MinimumSizeBuffer) {
    uint32_t keyId = importTestKey();

    // Buffer that is exactly the size of the security trailer: zero authenticated bytes
    std::vector<uint8_t> packet(Ccsds355_0_B_2::kTCSecurityTrailer, 0);
    Mac mac{};

    auto res = authenticateFrame(0, packet.data(), packet.size(), mac, keyId);
    EXPECT_EQ(res.status, PacketAuthenticator::AuthenticationStatus::VerifyError);
    EXPECT_EQ(res.psaStatus, PSA_ERROR_INVALID_SIGNATURE);
}

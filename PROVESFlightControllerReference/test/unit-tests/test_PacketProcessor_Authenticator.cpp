#include <gtest/gtest.h>
#include <psa/crypto.h>

#include "PROVESFlightControllerReference/Components/TcSecurityDeframer/Authenticator.hpp"

constexpr char kTestKeyHex[] =
    "14408c2711281f4d70452ce3730bb4fa";  //!< The hex-encoded key corresponding to the HMAC in the test packets

using namespace Components;

TEST(PacketAuthenticatorTest, NullBuffer) {
    Mac hmac{};
    auto res = Components::authenticatePacket(nullptr, 0, hmac, kTestKeyHex);
    EXPECT_EQ(res.status, PacketAuthenticator::Status::VerifyError);
    EXPECT_EQ(res.psaStatus, PSA_ERROR_INVALID_ARGUMENT);
}

TEST(PacketAuthenticatorTest, AuthenticatedSuccess) {
    // Hardcoded packet
    std::vector<uint8_t> packet = {1,    2,    3,    4,    5,    6,    7,    8,    9,    10,   11,
                                   12,   13,   14,   15,   16,   0x54, 0x92, 0x46, 0xAF, 0xF2, 0xEA,
                                   0x86, 0x7C, 0xEB, 0xBC, 0x38, 0x5D, 0x73, 0xF8, 0x94, 0x9C};
    Mac hmac =
        *reinterpret_cast<const Mac*>(packet.data() + packet.size() - Ccsds355_0_B_2::kTCSecurityTrailer);

    auto res = Components::authenticatePacket(packet.data(), packet.size(), hmac, kTestKeyHex);
    EXPECT_EQ(res.status, PacketAuthenticator::Status::Authenticated);
    EXPECT_EQ(res.psaStatus, PSA_SUCCESS);
}

TEST(PacketAuthenticatorTest, VerifyFailure) {
    // Hardcoded packet
    std::vector<uint8_t> packet = {1,    2,    3,    4,    5,    6,    7,    8,    9,    10,   11,
                                   12,   13,   14,   15,   16,   0x54, 0x92, 0x46, 0xAF, 0xF2, 0xEA,
                                   0x86, 0x7C, 0xEB, 0xBC, 0x38, 0x5D, 0x73, 0xF8, 0x94, 0x9C};
    Mac hmac =
        *reinterpret_cast<const Mac*>(packet.data() + packet.size() - Ccsds355_0_B_2::kTCSecurityTrailer);

    // Corrupt one byte
    hmac[0] ^= 0xFF;

    auto res = Components::authenticatePacket(packet.data(), packet.size(), hmac, kTestKeyHex);
    EXPECT_EQ(res.status, PacketAuthenticator::Status::VerifyError);
    EXPECT_NE(res.psaStatus, PSA_SUCCESS);
}

TEST(PacketAuthenticatorTest, InvalidKey) {
    // Hardcoded packet
    std::vector<uint8_t> packet = {1,    2,    3,    4,    5,    6,    7,    8,    9,    10,   11,
                                   12,   13,   14,   15,   16,   0x54, 0x92, 0x46, 0xAF, 0xF2, 0xEA,
                                   0x86, 0x7C, 0xEB, 0xBC, 0x38, 0x5D, 0x73, 0xF8, 0x94, 0x9C};
    Mac hmac =
        *reinterpret_cast<const Mac*>(packet.data() + packet.size() - Ccsds355_0_B_2::kTCSecurityTrailer);

    auto res = Components::authenticatePacket(packet.data(), packet.size(), hmac, "invalidkey");
    EXPECT_EQ(res.status, PacketAuthenticator::Status::ParseKeyError);
    EXPECT_EQ(res.psaStatus, PSA_ERROR_INVALID_ARGUMENT);
}

TEST(PacketAuthenticatorTest, NullKey) {
    // Hardcoded packet
    std::vector<uint8_t> packet = {1,    2,    3,    4,    5,    6,    7,    8,    9,    10,   11,
                                   12,   13,   14,   15,   16,   0x54, 0x92, 0x46, 0xAF, 0xF2, 0xEA,
                                   0x86, 0x7C, 0xEB, 0xBC, 0x38, 0x5D, 0x73, 0xF8, 0x94, 0x9C};
    Mac hmac =
        *reinterpret_cast<const Mac*>(packet.data() + packet.size() - Ccsds355_0_B_2::kTCSecurityTrailer);

    auto res = Components::authenticatePacket(packet.data(), packet.size(), hmac, nullptr);
    EXPECT_EQ(res.status, PacketAuthenticator::Status::ParseKeyError);
    EXPECT_EQ(res.psaStatus, PSA_ERROR_INVALID_ARGUMENT);
}

TEST(PacketAuthenticatorTest, ShortBuffer) {
    std::vector<uint8_t> packet = {1, 2, 3};  // Too short to contain a valid packet
    Mac hmac{};

    auto res = Components::authenticatePacket(packet.data(), packet.size(), hmac, kTestKeyHex);
    EXPECT_EQ(res.status, PacketAuthenticator::Status::VerifyError);
    EXPECT_EQ(res.psaStatus, PSA_ERROR_INVALID_ARGUMENT);
}

TEST(PacketAuthenticatorTest, MinimumSizeBuffer) {
    // Buffer that is exactly the size of the security trailer but contains no actual packet data
    std::vector<uint8_t> packet(Ccsds355_0_B_2::kTCSecurityTrailer, 0);
    Mac hmac{};

    auto res = Components::authenticatePacket(packet.data(), packet.size(), hmac, kTestKeyHex);
    EXPECT_EQ(res.status, PacketAuthenticator::Status::VerifyError);
    EXPECT_EQ(res.psaStatus, PSA_ERROR_INVALID_SIGNATURE);
}

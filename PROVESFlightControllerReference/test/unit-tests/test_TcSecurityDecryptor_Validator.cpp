#include <gtest/gtest.h>

#include "PROVESFlightControllerReference/Components/TcSecurityDecryptor/Validator.hpp"

using namespace Components;

TEST(PacketValidatorTest, ValidPacket) {
    // sa index 0 valid, seq 11 within window of expected 10
    auto res = validateFrame(0u, 11u, 10u, 5u);
    EXPECT_EQ(res, PacketValidator::Status::Valid);
}

TEST(PacketValidatorTest, SpiInvalid) {
    // non-zero SA index invalid
    auto res = validateFrame(1u, 11u, 10u, 5u);
    EXPECT_EQ(res, PacketValidator::Status::SpiInvalid);
}

TEST(PacketValidatorTest, SequenceNumberInvalid) {
    // sequence 20, expected 10, window 5 -> out
    auto res = validateFrame(0u, 20u, 10u, 5u);
    EXPECT_EQ(res, PacketValidator::Status::SequenceNumberInvalid);
}

TEST(PacketValidatorTest, SequenceNumberReplayRejected) {
    // below last accepted -> replay
    auto res = validateFrame(0u, 9u, 10u, 5u);
    EXPECT_EQ(res, PacketValidator::Status::SequenceNumberInvalid);
}

TEST(PacketValidatorTest, SequenceNumberWrapWithinWindow) {
    // expected near U32 max, sequence wraps to small value within window
    const uint32_t expected = 0xFFFFFFFEu;  // -2
    const uint32_t seq = 1u;                // wrapped value
    // distance = seq - expected = 1 - 0xFFFFFFFE = 3 (mod 2^32)
    auto res = validateFrame(0u, seq, expected, 5u);
    EXPECT_EQ(res, PacketValidator::Status::Valid);
}

TEST(PacketValidatorTest, SequenceNumberWrapOutOfWindow) {
    // expected near U32 max, sequence wraps to a value outside window
    const uint32_t expected = 0xFFFFFFF0u;  // -16
    const uint32_t seq = 10u;               // wrapped value
    // distance = 10 - 0xFFFFFFF0 = 26 (mod 2^32)
    auto res = validateFrame(0u, seq, expected, 5u);
    EXPECT_EQ(res, PacketValidator::Status::SequenceNumberInvalid);
}

TEST(PacketValidatorTest, SequenceNumberEqualToExpected) {
    // sequence equal to last accepted must be rejected (no reuse)
    auto res = validateFrame(0u, 10u, 10u, 5u);
    EXPECT_EQ(res, PacketValidator::Status::SequenceNumberInvalid);
}

TEST(PacketValidatorTest, SequenceNumberAtWindowBoundary) {
    // expected 10, window 5 -> distance = 5 = window -> valid
    auto res = validateFrame(0u, 15u, 10u, 5u);
    EXPECT_EQ(res, PacketValidator::Status::Valid);
}

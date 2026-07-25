#include <gtest/gtest.h>

#include "PROVESFlightControllerReference/Components/TcSecurityDeframer/Validator.hpp"

using namespace Components;
using Header = Ccsds355_0_B_2::TCSecurityHeader;

TEST(PacketValidatorTest, ValidPacket) {
    Header h{0u, 11u};  // spi 0 valid, seq 11 within window of expected 10
    auto res = validatePacket(h, 10u, 5u);
    EXPECT_EQ(res, PacketValidator::Status::Valid);
}

TEST(PacketValidatorTest, SpiInvalid) {
    Header h{1u, 11u};  // non-zero SPI invalid
    auto res = validatePacket(h, 10u, 5u);
    EXPECT_EQ(res, PacketValidator::Status::SpiInvalid);
}

TEST(PacketValidatorTest, SequenceNumberInvalid) {
    Header h{0u, 20u};  // sequence 20, expected 10, window 5 -> out
    auto res = validatePacket(h, 10u, 5u);
    EXPECT_EQ(res, PacketValidator::Status::SequenceNumberInvalid);
}

TEST(PacketValidatorTest, SequenceNumberReplayRejected) {
    Header h{0u, 9u};  // below last accepted -> replay
    auto res = validatePacket(h, 10u, 5u);
    EXPECT_EQ(res, PacketValidator::Status::SequenceNumberInvalid);
}

TEST(PacketValidatorTest, SequenceNumberWrapWithinWindow) {
    // expected near U32 max, sequence wraps to small value within window
    const uint32_t expected = 0xFFFFFFFEu;  // -2
    const uint32_t seq = 1u;                // wrapped value
    // distance = seq - expected = 1 - 0xFFFFFFFE = 3 (mod 2^32)
    Header h{0u, seq};

    auto res = validatePacket(h, expected, 5u);
    EXPECT_EQ(res, PacketValidator::Status::Valid);
}

TEST(PacketValidatorTest, SequenceNumberWrapOutOfWindow) {
    // expected near U32 max, sequence wraps to a value outside window
    const uint32_t expected = 0xFFFFFFF0u;  // -16
    const uint32_t seq = 10u;               // wrapped value
    // distance = 10 - 0xFFFFFFF0 = 26 (mod 2^32)
    Header h{0u, seq};

    auto res = validatePacket(h, expected, 5u);
    EXPECT_EQ(res, PacketValidator::Status::SequenceNumberInvalid);
}

TEST(PacketValidatorTest, SequenceNumberEqualToExpected) {
    Header h{0u, 10u};  // sequence equal to last accepted must be rejected (no reuse)
    auto res = validatePacket(h, 10u, 5u);
    EXPECT_EQ(res, PacketValidator::Status::SequenceNumberInvalid);
}

TEST(PacketValidatorTest, SequenceNumberAtWindowBoundary) {
    Header h{0u, 15u};  // expected 10, window 5 -> distance = 5 = window -> valid
    auto res = validatePacket(h, 10u, 5u);
    EXPECT_EQ(res, PacketValidator::Status::Valid);
}

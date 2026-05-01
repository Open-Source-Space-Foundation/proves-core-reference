#include <gtest/gtest.h>

#include "PROVESFlightControllerReference/Components/PacketProcessor/Validator.hpp"

using namespace Components;

TEST(PacketValidatorTest, ValidPacket) {
    Packet p{0u, 10u, 0xDEADBEEFu, {}};  // spi 0 valid
    auto res = validatePacket(p, 10u, 5u);
    EXPECT_EQ(res, PacketValidator::Status::Valid);
}

TEST(PacketValidatorTest, BypassOpCode) {
    Packet p{0u, 10u, 0x01000000u, {}};  // bypass opcode
    auto res = validatePacket(p, 10u, 5u);
    EXPECT_EQ(res, PacketValidator::Status::Bypass);
}

TEST(PacketValidatorTest, SpiInvalid) {
    Packet p{1u, 10u, 0xDEADBEEFu, {}};  // non-zero SPI invalid
    auto res = validatePacket(p, 10u, 5u);
    EXPECT_EQ(res, PacketValidator::Status::SpiInvalid);
}

TEST(PacketValidatorTest, SequenceNumberOutOfWindow) {
    Packet p{0u, 20u, 0xDEADBEEFu, {}};  // sequence 20, expected 10, window 5 -> out
    auto res = validatePacket(p, 10u, 5u);
    EXPECT_EQ(res, PacketValidator::Status::SequenceNumberOutOfWindow);
}

TEST(PacketValidatorTest, SequenceNumberWrapWithinWindow) {
    // expected near U32 max, sequence wraps to small value within window
    const uint32_t expected = 0xFFFFFFFEu;  // -2
    const uint32_t seq = 1u;                // wrapped value
    // distance = seq - expected = 1 - 0xFFFFFFFE = 3 (mod 2^32)
    Packet p{0u, seq, 0xDEADBEEFu, {}};

    auto res = validatePacket(p, expected, 5u);
    EXPECT_EQ(res, PacketValidator::Status::Valid);
}

TEST(PacketValidatorTest, SequenceNumberWrapOutOfWindow) {
    // expected near U32 max, sequence wraps to a value outside window
    const uint32_t expected = 0xFFFFFFF0u;  // -16
    const uint32_t seq = 10u;               // wrapped value
    // distance = 10 - 0xFFFFFFF0 = 26 (mod 2^32)
    Packet p{0u, seq, 0xDEADBEEFu, {}};

    auto res = validatePacket(p, expected, 5u);
    EXPECT_EQ(res, PacketValidator::Status::SequenceNumberOutOfWindow);
}

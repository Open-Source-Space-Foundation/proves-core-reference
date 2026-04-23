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

#include <gtest/gtest.h>

#include <vector>

#include "PROVESFlightControllerReference/Components/ProvesRouter/Bypasser.hpp"

using namespace Components::PacketBypasser;

namespace {

// Builds a minimal packet buffer with the given OpCode at bytes [2, 6).
std::vector<uint8_t> makePacket(uint32_t opCode, size_t totalSize = 6) {
    std::vector<uint8_t> buf(totalSize, 0);
    buf[2] = static_cast<uint8_t>((opCode >> 24) & 0xFF);
    buf[3] = static_cast<uint8_t>((opCode >> 16) & 0xFF);
    buf[4] = static_cast<uint8_t>((opCode >> 8) & 0xFF);
    buf[5] = static_cast<uint8_t>(opCode & 0xFF);
    return buf;
}

}  // namespace

TEST(BypasserTest, AllowsNoOp) {
    auto buf = makePacket(0x01000000);
    EXPECT_TRUE(bypassPacket(buf.data(), buf.size()));
}

TEST(BypasserTest, AllowsUartGetSeqNum) {
    auto buf = makePacket(0x2100B000);
    EXPECT_TRUE(bypassPacket(buf.data(), buf.size()));
}

TEST(BypasserTest, AllowsLoraGetSeqNum) {
    auto buf = makePacket(0x2200B000);
    EXPECT_TRUE(bypassPacket(buf.data(), buf.size()));
}

TEST(BypasserTest, AllowsSbandGetSeqNum) {
    auto buf = makePacket(0x2300B000);
    EXPECT_TRUE(bypassPacket(buf.data(), buf.size()));
}

TEST(BypasserTest, AllowsTellJoke) {
    auto buf = makePacket(0x10065000);
    EXPECT_TRUE(bypassPacket(buf.data(), buf.size()));
}

TEST(BypasserTest, RejectsUnknownOpCode) {
    auto buf = makePacket(0xDEADBEEF);
    EXPECT_FALSE(bypassPacket(buf.data(), buf.size()));
}

TEST(BypasserTest, RejectsNullBuffer) {
    EXPECT_FALSE(bypassPacket(nullptr, 6));
}

TEST(BypasserTest, RejectsBufferTooSmall) {
    // Buffer must be at least offset(2) + size(4) = 6 bytes.
    std::vector<uint8_t> buf(5, 0);
    EXPECT_FALSE(bypassPacket(buf.data(), buf.size()));
}

TEST(BypasserTest, RejectsZeroSizeBuffer) {
    EXPECT_FALSE(bypassPacket(nullptr, 0));
}

TEST(BypasserTest, AllowsWithExtraTrailingBytes) {
    // A larger packet with the OpCode still located at the expected offset should bypass.
    auto buf = makePacket(0x01000000, /*totalSize=*/32);
    EXPECT_TRUE(bypassPacket(buf.data(), buf.size()));
}

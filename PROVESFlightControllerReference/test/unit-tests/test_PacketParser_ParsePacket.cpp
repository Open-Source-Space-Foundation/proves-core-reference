#include <gtest/gtest.h>

#include "PROVESFlightControllerReference/Components/PacketProcessor/Parser.hpp"

using namespace Components;

TEST(PacketParserTest, SuccessfulParse) {
    // Minimal valid buffer size: 18 bytes before HMAC + 16 bytes HMAC = 34
    std::vector<uint8_t> buf(34, 0);

    // SPI (2 bytes)
    buf[0] = 0x12;
    buf[1] = 0x34;  // SPI = 0x1234

    // Sequence number (4 bytes big-endian)
    buf[2] = 0x01;
    buf[3] = 0x02;
    buf[4] = 0x03;
    buf[5] = 0x04;  // seq = 0x01020304

    // OpCode starts at index 14..17
    buf[14] = 0xAA;
    buf[15] = 0xBB;
    buf[16] = 0xCC;
    buf[17] = 0xDD;  // opCode = 0xAABBCCDD

    // HMAC occupies last 16 bytes (indices 18..33)
    for (size_t i = 0; i < 16; ++i) {
        buf[18 + i] = static_cast<uint8_t>(i);
    }

    auto res = parsePacket(buf.data(), buf.size());

    EXPECT_EQ(res.status, PacketParser::Status::Ok);
    EXPECT_EQ(res.packet.spi, 0x1234u);
    EXPECT_EQ(res.packet.sequenceNumber, 0x01020304u);
    EXPECT_EQ(res.packet.opCode, 0xAABBCCDDu);

    for (size_t i = 0; i < 16; ++i) {
        EXPECT_EQ(res.packet.hmac[i], static_cast<uint8_t>(i));
    }
}

TEST(PacketParserTest, SpiParseErrorTooSmall) {
    std::vector<uint8_t> buf(1, 0);  // smaller than SPI field requirement (2)

    auto res = parsePacket(buf.data(), buf.size());
    EXPECT_EQ(res.status, PacketParser::Status::SpiParseError);
}

TEST(PacketParserTest, OpCodeParseErrorTooShort) {
    // Make size large enough for header (spi + seq) but too short for opcode
    // opcode parsing requires at least 18 bytes total (kSecurityHeaderSize + 6 + 2 + 4)
    // choose 17 to be just below that threshold
    std::vector<uint8_t> buf(17, 0);

    buf[0] = 0x00;
    buf[1] = 0x01;
    buf[2] = 0x00;
    buf[3] = 0x00;
    buf[4] = 0x00;
    buf[5] = 0x02;

    auto res = parsePacket(buf.data(), buf.size());
    EXPECT_EQ(res.status, PacketParser::Status::OpCodeParseError);
}

TEST(PacketParserTest, SequenceNumberParseErrorTooShort) {
    // Make size large enough for SPI (2 bytes) but too short for sequence number (needs 6)
    std::vector<uint8_t> buf(3, 0);

    buf[0] = 0x00;
    buf[1] = 0x01;  // SPI present

    auto res = parsePacket(buf.data(), buf.size());
    EXPECT_EQ(res.status, PacketParser::Status::SequenceNumberParseError);
}

TEST(PacketParserTest, HmacParseErrorTooShort) {
    // Make size large enough for opcode parsing (>=18) but too short to contain HMAC (needs 34)
    std::vector<uint8_t> buf(20, 0);

    // Fill minimally for header and opcode checks
    buf[0] = 0x00;
    buf[1] = 0x01;
    buf[2] = 0x00;
    buf[3] = 0x00;
    buf[4] = 0x00;
    buf[5] = 0x02;
    // opcode area (offsets computed in implementation)
    buf[14] = 0x11;
    buf[15] = 0x22;
    buf[16] = 0x33;
    buf[17] = 0x44;

    auto res = parsePacket(buf.data(), buf.size());
    EXPECT_EQ(res.status, PacketParser::Status::HmacParseError);
}

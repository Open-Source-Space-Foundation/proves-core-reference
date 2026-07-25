#include <gtest/gtest.h>

#include <vector>

#include "PROVESFlightControllerReference/Components/TcSecurityDeframer/Parser.hpp"

using namespace Components;
using Parser = Ccsds355_0_B_2::TcTransferFrame::Parser::Status;

TEST(PacketParserTest, SuccessfulParse) {
    // Security header (6) + data field (12) + security trailer (16) = 34
    std::vector<uint8_t> buf(34, 0);

    // SPI (2 bytes)
    buf[0] = 0x12;
    buf[1] = 0x34;  // SPI = 0x1234

    // Sequence number (4 bytes big-endian)
    buf[2] = 0x01;
    buf[3] = 0x02;
    buf[4] = 0x03;
    buf[5] = 0x04;  // seq = 0x01020304

    // MAC occupies last 16 bytes (indices 18..33)
    for (size_t i = 0; i < 16; ++i) {
        buf[18 + i] = static_cast<uint8_t>(i);
    }

    auto res = Ccsds355_0_B_2::parse(buf.data(), buf.size());

    EXPECT_EQ(res.status, Parser::Ok);
    EXPECT_EQ(res.securityHeader.spi, 0x1234u);
    EXPECT_EQ(res.securityHeader.sequenceNumber, 0x01020304u);

    for (size_t i = 0; i < 16; ++i) {
        EXPECT_EQ(res.securityTrailer.mac[i], static_cast<uint8_t>(i));
    }

    // Frame data is everything between the security header and trailer
    EXPECT_EQ(res.frameData.data, buf.data() + Ccsds355_0_B_2::kTCSecurityHeaderSize);
    EXPECT_EQ(res.frameData.size,
              buf.size() - Ccsds355_0_B_2::kTCSecurityHeaderSize - Ccsds355_0_B_2::kTCSecurityTrailer);
}

TEST(PacketParserTest, NullBuffer) {
    auto res = Ccsds355_0_B_2::parse(nullptr, 34);
    EXPECT_EQ(res.status, Parser::SpiParseError);
}

TEST(PacketParserTest, SpiParseErrorTooSmall) {
    std::vector<uint8_t> buf(1, 0);  // smaller than SPI field requirement (2)

    auto res = Ccsds355_0_B_2::parse(buf.data(), buf.size());
    EXPECT_EQ(res.status, Parser::SpiParseError);
}

TEST(PacketParserTest, SequenceNumberParseErrorTooShort) {
    // Large enough for SPI (2 bytes) but too short for the full security header (6)
    std::vector<uint8_t> buf(3, 0);

    buf[0] = 0x00;
    buf[1] = 0x01;  // SPI present

    auto res = Ccsds355_0_B_2::parse(buf.data(), buf.size());
    EXPECT_EQ(res.status, Parser::SequenceNumberParseError);
}

TEST(PacketParserTest, MacParseErrorTooShort) {
    // Large enough for the security header (6) but too short to contain the MAC (needs 22)
    std::vector<uint8_t> buf(20, 0);

    buf[0] = 0x00;
    buf[1] = 0x01;
    buf[5] = 0x02;

    auto res = Ccsds355_0_B_2::parse(buf.data(), buf.size());
    EXPECT_EQ(res.status, Parser::MacParseError);
}

TEST(PacketParserTest, MinimumSizeEmptyDataField) {
    // Exactly header + trailer: parses with an empty data field
    std::vector<uint8_t> buf(Ccsds355_0_B_2::kMinAuthenticatedPacketSize, 0);

    auto res = Ccsds355_0_B_2::parse(buf.data(), buf.size());
    EXPECT_EQ(res.status, Parser::Ok);
    EXPECT_EQ(res.frameData.size, 0u);
}

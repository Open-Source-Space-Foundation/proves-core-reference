#include <gtest/gtest.h>

#include <vector>

#include "PROVESFlightControllerReference/Components/TcSecurityDecryptor/Parser.hpp"

using namespace Components;
using Parser = Ccsds355_0_B_2::TcTransferFrame::Parser::Status;

TEST(PacketParserTest, SuccessfulParse) {
    // Sequence number (4) + data field (12) + security trailer (16) = 32
    std::vector<uint8_t> buf(32, 0);

    // Sequence number (4 bytes big-endian)
    buf[0] = 0x01;
    buf[1] = 0x02;
    buf[2] = 0x03;
    buf[3] = 0x04;  // seq = 0x01020304

    // MAC occupies last 16 bytes (indices 16..31)
    for (size_t i = 0; i < 16; ++i) {
        buf[16 + i] = static_cast<uint8_t>(i);
    }

    auto res = Ccsds355_0_B_2::parse(buf.data(), buf.size());

    EXPECT_EQ(res.status, Parser::Ok);
    EXPECT_EQ(res.securityHeader.sequenceNumber, 0x01020304u);

    for (size_t i = 0; i < 16; ++i) {
        EXPECT_EQ(res.securityTrailer.mac[i], static_cast<uint8_t>(i));
    }

    // Frame data is everything between the sequence number and the trailer
    EXPECT_EQ(res.frameData.data, buf.data() + Ccsds355_0_B_2::kSequenceNumberSize);
    EXPECT_EQ(res.frameData.size,
              buf.size() - Ccsds355_0_B_2::kSequenceNumberSize - Ccsds355_0_B_2::kTCSecurityTrailer);
}

TEST(PacketParserTest, NullBuffer) {
    auto res = Ccsds355_0_B_2::parse(nullptr, 32);
    EXPECT_EQ(res.status, Parser::SequenceNumberParseError);
}

TEST(PacketParserTest, SequenceNumberParseErrorTooShort) {
    // Too short for the full sequence number (4)
    std::vector<uint8_t> buf(3, 0);

    auto res = Ccsds355_0_B_2::parse(buf.data(), buf.size());
    EXPECT_EQ(res.status, Parser::SequenceNumberParseError);
}

TEST(PacketParserTest, MacParseErrorTooShort) {
    // Large enough for the sequence number (4) but too short to contain the MAC (needs 20)
    std::vector<uint8_t> buf(10, 0);

    buf[3] = 0x02;

    auto res = Ccsds355_0_B_2::parse(buf.data(), buf.size());
    EXPECT_EQ(res.status, Parser::MacParseError);
}

TEST(PacketParserTest, MinimumSizeEmptyDataField) {
    // Exactly SeqNum + trailer: parses with an empty data field
    std::vector<uint8_t> buf(Ccsds355_0_B_2::kMinAuthenticatedPacketSize, 0);

    auto res = Ccsds355_0_B_2::parse(buf.data(), buf.size());
    EXPECT_EQ(res.status, Parser::Ok);
    EXPECT_EQ(res.frameData.size, 0u);
}

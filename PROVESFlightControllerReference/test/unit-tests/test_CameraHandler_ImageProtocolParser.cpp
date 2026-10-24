#include <gtest/gtest.h>

#include <cstdint>
#include <string>
#include <vector>

#include "PROVESFlightControllerReference/Components/CameraHandler/ImageProtocolParser.hpp"

using Components::ImageProtocolParser;
using Bytes = std::vector<std::uint8_t>;
using Found = ImageProtocolParser::Found;

namespace {

Bytes text(const std::string& s) {
    return Bytes(s.begin(), s.end());
}

Bytes concat(Bytes a, const Bytes& b) {
    a.insert(a.end(), b.begin(), b.end());
    return a;
}

// <IMG_START><SIZE>[size u32 LE]</SIZE>
Bytes header(std::uint32_t size) {
    Bytes bytes = text("<IMG_START><SIZE>");
    for (int i = 0; i < 4; i++) {
        bytes.push_back(static_cast<std::uint8_t>((size >> (8 * i)) & 0xFFU));
    }
    const Bytes close = text("</SIZE>");
    bytes.insert(bytes.end(), close.begin(), close.end());
    return bytes;
}

ImageProtocolParser::Result feed(ImageProtocolParser& parser, const Bytes& bytes) {
    return parser.feed(bytes.data(), static_cast<std::uint32_t>(bytes.size()));
}

// Starts a transfer of the given size and takes the (empty) data that followed the header
void startTransfer(ImageProtocolParser& parser, std::uint32_t size) {
    ASSERT_EQ(feed(parser, header(size)).found, Found::IMAGE_HEADER);
    parser.takeBufferedImageData();
    parser.clearBuffer();
}

}  // namespace

// ----------------------------------------------------------------------
// Header
// ----------------------------------------------------------------------

TEST(ImageProtocolParserHeaderTest, HeaderIs28Bytes) {
    // Copied, because EXPECT_EQ takes a reference and a constexpr member is not defined before C++17
    const std::uint32_t headerSize = ImageProtocolParser::HEADER_SIZE;
    EXPECT_EQ(header(0).size(), headerSize);
}

TEST(ImageProtocolParserHeaderTest, ValidHeaderStartsTransfer) {
    ImageProtocolParser parser;

    const auto result = feed(parser, header(1000));

    EXPECT_EQ(result.found, Found::IMAGE_HEADER);
    EXPECT_EQ(result.imageSize, 1000u);
    EXPECT_TRUE(parser.isReceiving());
    EXPECT_EQ(parser.expectedSize(), 1000u);
    EXPECT_EQ(parser.bytesReceived(), 0u);
}

TEST(ImageProtocolParserHeaderTest, SizeIsLittleEndian) {
    ImageProtocolParser parser;

    EXPECT_EQ(feed(parser, header(0x01020304u)).imageSize, 0x01020304u);
}

TEST(ImageProtocolParserHeaderTest, HeaderAtNonZeroOffset) {
    ImageProtocolParser parser;

    const auto result = feed(parser, concat(text("some text\r\n"), header(77)));

    EXPECT_EQ(result.found, Found::IMAGE_HEADER);
    EXPECT_EQ(result.imageSize, 77u);
}

TEST(ImageProtocolParserHeaderTest, HeaderSplitAcrossChunks) {
    ImageProtocolParser parser;
    const Bytes full = header(4096);

    for (std::size_t split = 1; split < full.size(); split++) {
        ImageProtocolParser split_parser;
        const Bytes first(full.begin(), full.begin() + static_cast<std::ptrdiff_t>(split));
        const Bytes second(full.begin() + static_cast<std::ptrdiff_t>(split), full.end());

        EXPECT_EQ(feed(split_parser, first).found, Found::NOTHING) << "split at " << split;
        const auto result = feed(split_parser, second);
        EXPECT_EQ(result.found, Found::IMAGE_HEADER) << "split at " << split;
        EXPECT_EQ(result.imageSize, 4096u) << "split at " << split;
    }
}

TEST(ImageProtocolParserHeaderTest, HeaderAfterNoiseInEarlierChunks) {
    ImageProtocolParser parser;

    EXPECT_EQ(feed(parser, text("boot message one\r\n")).found, Found::NOTHING);
    EXPECT_EQ(feed(parser, text("boot message two\r\n")).found, Found::NOTHING);
    EXPECT_EQ(feed(parser, header(5)).found, Found::IMAGE_HEADER);
}

TEST(ImageProtocolParserHeaderTest, ZeroSizeImage) {
    ImageProtocolParser parser;

    const auto result = feed(parser, header(0));

    EXPECT_EQ(result.found, Found::IMAGE_HEADER);
    EXPECT_EQ(result.imageSize, 0u);
    EXPECT_TRUE(parser.isReceiving());
    // Already complete, but nothing finalizes it until the next chunk arrives
    EXPECT_TRUE(parser.isComplete());
    EXPECT_TRUE(parser.planChunk(10).complete);
}

TEST(ImageProtocolParserHeaderTest, BadSizeOpenTagIsNotAHeader) {
    ImageProtocolParser parser;
    Bytes bad = header(10);
    bad[11] = 'X';

    EXPECT_EQ(feed(parser, bad).found, Found::NOTHING);
    EXPECT_FALSE(parser.isReceiving());
}

TEST(ImageProtocolParserHeaderTest, BadSizeCloseTagIsNotAHeader) {
    ImageProtocolParser parser;
    Bytes bad = header(10);
    bad[21] = 'X';

    EXPECT_EQ(feed(parser, bad).found, Found::NOTHING);
    EXPECT_FALSE(parser.isReceiving());
}

TEST(ImageProtocolParserHeaderTest, BadHeaderStaysInBufferUntilTrimmed) {
    ImageProtocolParser parser;
    Bytes bad = header(10);
    bad[11] = 'X';
    feed(parser, bad);

    // A valid header behind the bad one is not found: the bad one is still at the start of the buffer
    EXPECT_EQ(feed(parser, header(20)).found, Found::NOTHING);
}

// ----------------------------------------------------------------------
// PONG
// ----------------------------------------------------------------------

TEST(ImageProtocolParserPongTest, PongIsFound) {
    ImageProtocolParser parser;

    EXPECT_EQ(feed(parser, text("PONG\n")).found, Found::PONG);
}

TEST(ImageProtocolParserPongTest, PongInsideLongerText) {
    ImageProtocolParser parser;

    EXPECT_EQ(feed(parser, text("ok PONG ok")).found, Found::PONG);
}

TEST(ImageProtocolParserPongTest, PongIsNotConsumed) {
    ImageProtocolParser parser;

    EXPECT_EQ(feed(parser, text("PONG\n")).found, Found::PONG);
    // The next chunk still sees the old PONG
    EXPECT_EQ(feed(parser, text("x")).found, Found::PONG);
}

TEST(ImageProtocolParserPongTest, HeaderWinsOverPong) {
    ImageProtocolParser parser;

    EXPECT_EQ(feed(parser, concat(text("PONG"), header(3))).found, Found::IMAGE_HEADER);
}

// ----------------------------------------------------------------------
// Buffer limits
// ----------------------------------------------------------------------

TEST(ImageProtocolParserBufferTest, TextOverHalfTheBufferIsTrimmedToItsTail) {
    ImageProtocolParser parser;

    // 70 bytes of text without a header, then a header that is cut at a chunk edge
    EXPECT_EQ(feed(parser, Bytes(70, 'a')).found, Found::NOTHING);
    // After trimming only the last 16 bytes remain, so a header started in them survives
    const Bytes full = header(9);
    const Bytes tail(full.begin(), full.begin() + 10);
    const Bytes rest(full.begin() + 10, full.end());
    EXPECT_EQ(feed(parser, tail).found, Found::NOTHING);
    EXPECT_EQ(feed(parser, rest).found, Found::IMAGE_HEADER);
}

TEST(ImageProtocolParserBufferTest, TextWithoutHeaderLeavesRoomForAHeader) {
    ImageProtocolParser parser;

    // 116 bytes without a header is over half the buffer, so it is trimmed to its last 16 bytes
    EXPECT_EQ(feed(parser, Bytes(116, 'a')).found, Found::NOTHING);
    // A header fits afterwards because the buffer was trimmed
    EXPECT_EQ(feed(parser, header(1)).found, Found::IMAGE_HEADER);
}

TEST(ImageProtocolParserBufferTest, OversizedChunkKeepsItsFirst128Bytes) {
    ImageProtocolParser parser;
    Bytes big = header(500);
    big.resize(200, 'z');

    const auto result = feed(parser, big);

    EXPECT_EQ(result.found, Found::IMAGE_HEADER);
    EXPECT_EQ(result.imageSize, 500u);
    const auto chunk = parser.takeBufferedImageData();
    EXPECT_TRUE(chunk.present);
    EXPECT_EQ(chunk.size, ImageProtocolParser::PROTOCOL_BUFFER_SIZE - ImageProtocolParser::HEADER_SIZE);
}

TEST(ImageProtocolParserBufferTest, RepeatedTextDoesNotBlockLaterHeader) {
    ImageProtocolParser parser;
    // 100 bytes without a header is trimmed to 16, so the next 100 bytes still fit
    feed(parser, Bytes(100, 'a'));

    EXPECT_EQ(feed(parser, Bytes(100, 'b')).found, Found::NOTHING);
    EXPECT_EQ(feed(parser, header(8)).found, Found::IMAGE_HEADER);
}

// ----------------------------------------------------------------------
// Data that follows the header
// ----------------------------------------------------------------------

TEST(ImageProtocolParserBufferedDataTest, NoDataAfterHeader) {
    ImageProtocolParser parser;
    feed(parser, header(100));

    const auto chunk = parser.takeBufferedImageData();

    EXPECT_FALSE(chunk.present);
    EXPECT_EQ(chunk.size, 0u);
}

TEST(ImageProtocolParserBufferedDataTest, DataAfterHeaderIsReturned) {
    ImageProtocolParser parser;
    feed(parser, concat(header(100), text("abc")));

    const auto chunk = parser.takeBufferedImageData();

    ASSERT_TRUE(chunk.present);
    ASSERT_EQ(chunk.size, 3u);
    EXPECT_EQ(std::string(chunk.data, chunk.data + chunk.size), "abc");
}

TEST(ImageProtocolParserBufferedDataTest, DataPastTheExpectedSizeIsNotReturned) {
    ImageProtocolParser parser;
    feed(parser, concat(header(2), text("abcdef")));

    const auto chunk = parser.takeBufferedImageData();

    EXPECT_TRUE(chunk.present);
    EXPECT_EQ(chunk.size, 2u);
}

TEST(ImageProtocolParserBufferedDataTest, ZeroSizeImageWithTrailingDataHasNothingToWrite) {
    ImageProtocolParser parser;
    feed(parser, concat(header(0), text("abc")));

    const auto chunk = parser.takeBufferedImageData();

    // present but empty: the component's zero-byte write is reported as a file error
    EXPECT_TRUE(chunk.present);
    EXPECT_EQ(chunk.size, 0u);
}

TEST(ImageProtocolParserBufferedDataTest, ImageCompleteFromBufferedDataAlone) {
    ImageProtocolParser parser;
    feed(parser, concat(header(3), text("abc")));

    const auto chunk = parser.takeBufferedImageData();
    parser.onBufferedDataWritten(chunk.size);

    EXPECT_TRUE(parser.isComplete());
}

TEST(ImageProtocolParserBufferedDataTest, RejectedHeaderEndsTransferAndClearsBuffer) {
    ImageProtocolParser parser;
    feed(parser, concat(header(3), text("abc")));

    parser.rejectHeader();

    EXPECT_FALSE(parser.isReceiving());
    EXPECT_EQ(parser.expectedSize(), 0u);
    EXPECT_FALSE(parser.takeBufferedImageData().present);
}

// ----------------------------------------------------------------------
// Chunks
// ----------------------------------------------------------------------

TEST(ImageProtocolParserChunkTest, ChunkIsWrittenInFull) {
    ImageProtocolParser parser;
    startTransfer(parser, 100);

    const auto plan = parser.planChunk(40);

    EXPECT_FALSE(plan.complete);
    EXPECT_EQ(plan.toWrite, 40u);
}

TEST(ImageProtocolParserChunkTest, ChunkIsCappedAtTheRemainingBytes) {
    ImageProtocolParser parser;
    startTransfer(parser, 100);
    parser.onChunkWritten(90);

    const auto plan = parser.planChunk(40);

    EXPECT_FALSE(plan.complete);
    EXPECT_EQ(plan.toWrite, 10u);
}

TEST(ImageProtocolParserChunkTest, ChunkReachingTheExpectedSizeDoesNotComplete) {
    ImageProtocolParser parser;
    startTransfer(parser, 100);

    const auto plan = parser.planChunk(100);
    EXPECT_FALSE(plan.complete);
    EXPECT_EQ(plan.toWrite, 100u);

    parser.onChunkWritten(plan.toWrite);

    // Every byte is there, but the image is only finalized when the next chunk arrives
    EXPECT_TRUE(parser.isReceiving());
    EXPECT_TRUE(parser.isComplete());
    EXPECT_TRUE(parser.planChunk(1).complete);
}

TEST(ImageProtocolParserChunkTest, CompletePlanWritesNothing) {
    ImageProtocolParser parser;
    startTransfer(parser, 10);
    parser.onChunkWritten(10);

    EXPECT_EQ(parser.planChunk(50).toWrite, 0u);
}

TEST(ImageProtocolParserChunkTest, DataAfterTheImageIsParsedAsProtocol) {
    ImageProtocolParser parser;
    startTransfer(parser, 10);
    parser.onChunkWritten(10);
    parser.finishTransfer();

    const Bytes next = concat(text("<IMG_END>"), header(55));
    const auto result = parser.feedAfterImage(next.data(), static_cast<std::uint32_t>(next.size()));

    EXPECT_EQ(result.found, Found::IMAGE_HEADER);
    EXPECT_EQ(result.imageSize, 55u);
}

TEST(ImageProtocolParserChunkTest, DataAfterTheImageThatDoesNotFitIsDropped) {
    ImageProtocolParser parser;
    const Bytes big(200, 'x');

    EXPECT_EQ(parser.feedAfterImage(big.data(), static_cast<std::uint32_t>(big.size())).found, Found::NOTHING);
    EXPECT_EQ(feed(parser, header(1)).found, Found::IMAGE_HEADER);
}

// ----------------------------------------------------------------------
// Progress milestones
// ----------------------------------------------------------------------

TEST(ImageProtocolParserMilestoneTest, ReportsEachQuarter) {
    ImageProtocolParser parser;
    startTransfer(parser, 100);

    EXPECT_EQ(parser.onChunkWritten(10), 0);
    EXPECT_EQ(parser.onChunkWritten(15), 25);
    EXPECT_EQ(parser.onChunkWritten(10), 0);
    EXPECT_EQ(parser.onChunkWritten(15), 50);
    EXPECT_EQ(parser.onChunkWritten(25), 75);
    EXPECT_EQ(parser.onChunkWritten(25), 0);
}

TEST(ImageProtocolParserMilestoneTest, ExactlyOnTheBoundaryCounts) {
    ImageProtocolParser parser;
    startTransfer(parser, 100);

    EXPECT_EQ(parser.onChunkWritten(24), 0);
    EXPECT_EQ(parser.onChunkWritten(1), 25);
}

TEST(ImageProtocolParserMilestoneTest, LargeJumpReportsOnlyOneMilestonePerChunk) {
    ImageProtocolParser parser;
    startTransfer(parser, 100);

    // 0% to 80% in one chunk: only 25% is reported
    EXPECT_EQ(parser.onChunkWritten(80), 25);
    // 50% and 75% are caught up one chunk at a time, even if the chunk adds little
    EXPECT_EQ(parser.onChunkWritten(1), 50);
    EXPECT_EQ(parser.onChunkWritten(1), 75);
    EXPECT_EQ(parser.onChunkWritten(1), 0);
}

TEST(ImageProtocolParserMilestoneTest, MilestonesRestartForTheNextImage) {
    ImageProtocolParser parser;
    startTransfer(parser, 100);
    EXPECT_EQ(parser.onChunkWritten(30), 25);
    parser.finishTransfer();

    startTransfer(parser, 100);
    EXPECT_EQ(parser.onChunkWritten(30), 25);
}

TEST(ImageProtocolParserMilestoneTest, NoMilestonesForZeroSizeImage) {
    ImageProtocolParser parser;
    startTransfer(parser, 0);

    EXPECT_EQ(parser.onChunkWritten(0), 0);
}

TEST(ImageProtocolParserMilestoneTest, LargeImageDoesNotOverflow) {
    ImageProtocolParser parser;
    startTransfer(parser, 0xFFFFFFF0u);

    EXPECT_EQ(parser.onChunkWritten(0x40000000u), 25);
}

// ----------------------------------------------------------------------
// Ending a transfer
// ----------------------------------------------------------------------

TEST(ImageProtocolParserEndTest, FinishResetsProgress) {
    ImageProtocolParser parser;
    startTransfer(parser, 100);
    parser.onChunkWritten(50);

    parser.finishTransfer();

    EXPECT_FALSE(parser.isReceiving());
    EXPECT_EQ(parser.bytesReceived(), 0u);
    EXPECT_EQ(parser.expectedSize(), 0u);
}

TEST(ImageProtocolParserEndTest, AbortAlsoClearsBuffer) {
    ImageProtocolParser parser;
    feed(parser, concat(header(100), text("abc")));

    parser.abortTransfer();

    EXPECT_FALSE(parser.isReceiving());
    // The old header is gone, so a new one is found
    EXPECT_EQ(feed(parser, header(7)).imageSize, 7u);
}

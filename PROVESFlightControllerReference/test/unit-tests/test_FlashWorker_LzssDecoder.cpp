// ======================================================================
// \title  test_FlashWorker_LzssDecoder.cpp
// \brief  Unit tests for the delta patch transport codec
// ======================================================================

#include <gtest/gtest.h>

#include <cstring>
#include <vector>

#include "PROVESFlightControllerReference/Components/FlashWorker/LzssDecoder.hpp"

using Components::LzssDecoder;

namespace {

class MemSource final : public LzssDecoder::Source {
  public:
    explicit MemSource(std::vector<uint8_t> data) : m_data(std::move(data)) {}
    bool read(uint8_t* buffer, size_t size) override {
        if (m_position + size > m_data.size()) {
            return false;
        }
        std::memcpy(buffer, m_data.data() + m_position, size);
        m_position += size;
        return true;
    }

  private:
    std::vector<uint8_t> m_data;
    size_t m_position = 0;
};

class MemSink final : public LzssDecoder::Sink {
  public:
    bool write(const uint8_t* buffer, size_t size) override {
        if (m_fail) {
            return false;
        }
        out.insert(out.end(), buffer, buffer + size);
        return true;
    }
    void fail() { m_fail = true; }
    std::vector<uint8_t> out;

  private:
    bool m_fail = false;
};

//! Encode one group of up to 8 tokens. Each token is either a literal byte or a (distance, length)
//! match, matching what tools/bin/make-patch.py emits.
struct Token {
    bool literal;
    uint16_t value;  // literal byte, or distance
    uint16_t length;
};

std::vector<uint8_t> encode(const std::vector<Token>& tokens) {
    std::vector<uint8_t> out;
    for (size_t start = 0; start < tokens.size(); start += 8) {
        uint8_t tag = 0;
        for (size_t bit = 0; bit < 8 && start + bit < tokens.size(); bit++) {
            if (tokens[start + bit].literal) {
                tag |= static_cast<uint8_t>(1U << bit);
            }
        }
        out.push_back(tag);
        for (size_t bit = 0; bit < 8 && start + bit < tokens.size(); bit++) {
            const Token& token = tokens[start + bit];
            if (token.literal) {
                out.push_back(static_cast<uint8_t>(token.value));
            } else {
                out.push_back(static_cast<uint8_t>(token.value & 0xFF));
                out.push_back(static_cast<uint8_t>(token.value >> 8));
                out.push_back(static_cast<uint8_t>(token.length - LzssDecoder::MIN_MATCH));
            }
        }
    }
    return out;
}

LzssDecoder::Error run(const std::vector<Token>& tokens, uint32_t expected, std::vector<uint8_t>& out) {
    MemSource source(encode(tokens));
    MemSink sink;
    std::vector<uint8_t> window(LzssDecoder::WINDOW_SIZE);
    const LzssDecoder::Error error = LzssDecoder::decode(source, sink, expected, window.data());
    out = sink.out;
    return error;
}

}  // namespace

TEST(LzssDecoderTest, DecodesLiterals) {
    std::vector<uint8_t> out;
    EXPECT_EQ(LzssDecoder::Error::NONE, run({{true, 'a', 0}, {true, 'b', 0}, {true, 'c', 0}}, 3, out));
    EXPECT_EQ(std::vector<uint8_t>({'a', 'b', 'c'}), out);
}

TEST(LzssDecoderTest, DecodesAMatch) {
    std::vector<uint8_t> out;
    // "abc" then copy 3 bytes from 3 back
    EXPECT_EQ(LzssDecoder::Error::NONE, run({{true, 'a', 0}, {true, 'b', 0}, {true, 'c', 0}, {false, 3, 3}}, 6, out));
    EXPECT_EQ(std::vector<uint8_t>({'a', 'b', 'c', 'a', 'b', 'c'}), out);
}

TEST(LzssDecoderTest, OverlappingMatchProducesARun) {
    // Distance 1 with length 5 repeats the previous byte, which is how the zero runs that dominate
    // a patch are encoded. Copying byte-at-a-time rather than block-at-a-time is what makes this
    // work, so this is the case that would break a memcpy-based decoder.
    std::vector<uint8_t> out;
    EXPECT_EQ(LzssDecoder::Error::NONE, run({{true, 0, 0}, {false, 1, 5}}, 6, out));
    EXPECT_EQ(std::vector<uint8_t>(6, 0), out);
}

TEST(LzssDecoderTest, StopsAtTheExpectedSizeMidGroup) {
    // The final group is padded to 8 tokens; anything past the expected size must be ignored
    std::vector<uint8_t> out;
    EXPECT_EQ(LzssDecoder::Error::NONE, run({{true, 'x', 0}, {true, 'y', 0}, {true, 'z', 0}}, 2, out));
    EXPECT_EQ(std::vector<uint8_t>({'x', 'y'}), out);
}

TEST(LzssDecoderTest, RejectsADistanceBeyondWhatHasBeenProduced) {
    // A corrupt or hostile stream must not read outside the window
    std::vector<uint8_t> out;
    EXPECT_EQ(LzssDecoder::Error::BAD_DISTANCE, run({{true, 'a', 0}, {false, 8, 3}}, 4, out));
}

TEST(LzssDecoderTest, RejectsAZeroDistance) {
    std::vector<uint8_t> out;
    EXPECT_EQ(LzssDecoder::Error::BAD_DISTANCE, run({{true, 'a', 0}, {false, 0, 3}}, 4, out));
}

TEST(LzssDecoderTest, RejectsAMatchRunningPastTheExpectedSize) {
    std::vector<uint8_t> out;
    EXPECT_EQ(LzssDecoder::Error::OUTPUT_OVERRUN, run({{true, 'a', 0}, {false, 1, 10}}, 3, out));
}

TEST(LzssDecoderTest, ReportsATruncatedStream) {
    std::vector<uint8_t> out;
    MemSource source(std::vector<uint8_t>{});
    MemSink sink;
    std::vector<uint8_t> window(LzssDecoder::WINDOW_SIZE);
    EXPECT_EQ(LzssDecoder::Error::TRUNCATED_INPUT, LzssDecoder::decode(source, sink, 4, window.data()));
}

TEST(LzssDecoderTest, PropagatesOutputFailures) {
    MemSource source(encode({{true, 'a', 0}}));
    MemSink sink;
    sink.fail();
    std::vector<uint8_t> window(LzssDecoder::WINDOW_SIZE);
    EXPECT_EQ(LzssDecoder::Error::OUTPUT_WRITE_FAILED, LzssDecoder::decode(source, sink, 1, window.data()));
}

TEST(LzssDecoderTest, RejectsAMissingWindow) {
    MemSource source(encode({{true, 'a', 0}}));
    MemSink sink;
    EXPECT_EQ(LzssDecoder::Error::OUTPUT_WRITE_FAILED, LzssDecoder::decode(source, sink, 1, nullptr));
}

TEST(LzssDecoderTest, AnEmptyStreamNeedsNoInput) {
    MemSource source(std::vector<uint8_t>{});
    MemSink sink;
    std::vector<uint8_t> window(LzssDecoder::WINDOW_SIZE);
    EXPECT_EQ(LzssDecoder::Error::NONE, LzssDecoder::decode(source, sink, 0, window.data()));
    EXPECT_TRUE(sink.out.empty());
}

TEST(LzssDecoderTest, MatchesReachAcrossTheWindowBoundary) {
    // Produce more than a full window so the ring buffer wraps, then match recent history
    std::vector<Token> tokens;
    for (size_t i = 0; i < 200; i++) {
        tokens.push_back({true, static_cast<uint16_t>('A' + (i % 26)), 0});
    }
    // Repeat the last 104 bytes until well past WINDOW_SIZE. 104 is a multiple of the 26 letter
    // cycle, so the pattern continues seamlessly and any ring buffer wrap shows up as a mismatch.
    for (size_t i = 0; i < 60; i++) {
        tokens.push_back({false, 104, 104});
    }
    std::vector<uint8_t> out;
    const uint32_t expected = 200 + 60 * 104;
    ASSERT_GT(expected, LzssDecoder::WINDOW_SIZE);
    EXPECT_EQ(LzssDecoder::Error::NONE, run(tokens, expected, out));
    ASSERT_EQ(expected, out.size());
    // Every byte must still follow the original repeating pattern
    for (size_t i = 0; i < out.size(); i++) {
        EXPECT_EQ('A' + (i % 26), out[i]) << "at " << i;
    }
}

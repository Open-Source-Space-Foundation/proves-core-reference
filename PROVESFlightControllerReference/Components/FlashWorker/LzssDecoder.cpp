// ======================================================================
// \title  LzssDecoder.cpp
// \brief  cpp file for decoding the LZ77 stream a delta patch is carried in
// ======================================================================

#include "LzssDecoder.hpp"

namespace Components {

// Out of line definitions so these can be odr-used by callers; not every translation unit here is
// compiled as C++17, where static constexpr members would be implicitly inline.
constexpr size_t LzssDecoder::WINDOW_SIZE;
constexpr uint8_t LzssDecoder::MIN_MATCH;

LzssDecoder::Error LzssDecoder ::decode(Source& source, Sink& sink, uint32_t expected_size, uint8_t* window) {
    if (window == nullptr) {
        return Error::OUTPUT_WRITE_FAILED;
    }

    uint32_t produced = 0;
    // Position in the ring buffer where the next produced byte goes
    size_t cursor = 0;

    while (produced < expected_size) {
        uint8_t tag = 0;
        if (!source.read(&tag, 1)) {
            return Error::TRUNCATED_INPUT;
        }

        for (uint8_t bit = 0; bit < 8; bit++) {
            if (produced >= expected_size) {
                // The final group is padded; ignore whatever follows the last real token
                break;
            }

            if ((tag & (1U << bit)) != 0) {
                // Literal
                uint8_t value = 0;
                if (!source.read(&value, 1)) {
                    return Error::TRUNCATED_INPUT;
                }
                if (!sink.write(&value, 1)) {
                    return Error::OUTPUT_WRITE_FAILED;
                }
                window[cursor] = value;
                cursor = (cursor + 1) % LzssDecoder::WINDOW_SIZE;
                produced++;
                continue;
            }

            // Match: distance back, then length
            uint8_t encoded[3] = {0, 0, 0};
            if (!source.read(encoded, sizeof(encoded))) {
                return Error::TRUNCATED_INPUT;
            }
            const size_t distance = static_cast<size_t>(encoded[0]) | (static_cast<size_t>(encoded[1]) << 8);
            const size_t length = static_cast<size_t>(encoded[2]) + LzssDecoder::MIN_MATCH;

            if ((distance == 0) || (distance > LzssDecoder::WINDOW_SIZE) ||
                (distance > static_cast<size_t>(produced))) {
                return Error::BAD_DISTANCE;
            }
            if (length > (expected_size - produced)) {
                return Error::OUTPUT_OVERRUN;
            }

            // Copied one byte at a time on purpose: a match is allowed to overlap the bytes it is
            // producing, which is how a run is encoded, so the source advances with the output
            size_t from = (cursor + LzssDecoder::WINDOW_SIZE - distance) % LzssDecoder::WINDOW_SIZE;
            for (size_t i = 0; i < length; i++) {
                const uint8_t value = window[from];
                if (!sink.write(&value, 1)) {
                    return Error::OUTPUT_WRITE_FAILED;
                }
                window[cursor] = value;
                cursor = (cursor + 1) % LzssDecoder::WINDOW_SIZE;
                from = (from + 1) % LzssDecoder::WINDOW_SIZE;
            }
            produced += static_cast<uint32_t>(length);
        }
    }
    return Error::NONE;
}

}  // namespace Components

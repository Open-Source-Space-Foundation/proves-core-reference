// ======================================================================
// \title  ImageProtocolParser.cpp
// \brief  cpp file for ImageProtocolParser class
// ======================================================================

#include "PROVESFlightControllerReference/Components/CameraHandler/ImageProtocolParser.hpp"

#include <algorithm>
#include <cstring>

namespace Components {

namespace {

constexpr char IMG_START[] = "<IMG_START>";
constexpr char SIZE_OPEN[] = "<SIZE>";
constexpr char SIZE_CLOSE[] = "</SIZE>";
constexpr char PONG[] = "PONG";

// Lengths without the terminating NUL
constexpr std::uint32_t IMG_START_LEN = sizeof(IMG_START) - 1;
constexpr std::uint32_t SIZE_OPEN_LEN = sizeof(SIZE_OPEN) - 1;
constexpr std::uint32_t SIZE_CLOSE_LEN = sizeof(SIZE_CLOSE) - 1;
constexpr std::uint32_t PONG_LEN = sizeof(PONG) - 1;
constexpr std::uint32_t SIZE_VALUE_LEN = 4;  // little-endian uint32

constexpr std::uint32_t SIZE_OPEN_OFFSET = IMG_START_LEN;
constexpr std::uint32_t SIZE_VALUE_OFFSET = SIZE_OPEN_OFFSET + SIZE_OPEN_LEN;
constexpr std::uint32_t SIZE_CLOSE_OFFSET = SIZE_VALUE_OFFSET + SIZE_VALUE_LEN;

static_assert(SIZE_CLOSE_OFFSET + SIZE_CLOSE_LEN == ImageProtocolParser::HEADER_SIZE, "header layout");

// Drop old data when more than 90% of the buffer is used, keeping the tail
constexpr std::uint32_t NEARLY_FULL = ImageProtocolParser::PROTOCOL_BUFFER_SIZE * 9 / 10;
constexpr std::uint32_t KEEP_WHEN_NEARLY_FULL = 32;

// Drop old data when more than half of the buffer is used and holds no header, keeping the tail
constexpr std::uint32_t HALF_FULL = ImageProtocolParser::PROTOCOL_BUFFER_SIZE / 2;
constexpr std::uint32_t KEEP_WHEN_HALF_FULL = 16;

}  // namespace

// ----------------------------------------------------------------------
// Construction and destruction
// ----------------------------------------------------------------------

ImageProtocolParser ::ImageProtocolParser()
    : m_buffer(), m_bufferSize(0), m_receiving(false), m_bytesReceived(0), m_expectedSize(0), m_lastMilestone(0) {}

ImageProtocolParser ::~ImageProtocolParser() {}

// ----------------------------------------------------------------------
// Public helper methods
// ----------------------------------------------------------------------

ImageProtocolParser::Result ImageProtocolParser ::feed(const std::uint8_t* data, std::uint32_t size) {
    // Protects against text responses that are not image headers filling the buffer
    if (this->m_bufferSize > NEARLY_FULL) {
        this->keepLast(KEEP_WHEN_NEARLY_FULL);
    }

    if (!this->accumulate(data, size)) {
        // Overflow: drop the old data and keep the new
        this->clearBuffer();
        if (!this->accumulate(data, size)) {
            // Still does not fit, keep what fits
            std::memcpy(this->m_buffer.data(), data, PROTOCOL_BUFFER_SIZE);
            this->m_bufferSize = PROTOCOL_BUFFER_SIZE;
        }
    }

    return this->process();
}

ImageProtocolParser::Result ImageProtocolParser ::feedAfterImage(const std::uint8_t* data, std::uint32_t size) {
    if (!this->accumulate(data, size)) {
        return Result{Found::NOTHING, 0};
    }
    return this->process();
}

void ImageProtocolParser ::rejectHeader() {
    this->m_receiving = false;
    this->m_expectedSize = 0;
    this->clearBuffer();
}

ImageProtocolParser::Chunk ImageProtocolParser ::takeBufferedImageData() {
    // Without a header in the buffer (rejected or never found) there is nothing to take
    if (this->m_bufferSize < HEADER_SIZE) {
        return Chunk{this->m_buffer.data(), 0, false};
    }

    const std::uint32_t remaining = this->m_bufferSize - HEADER_SIZE;
    if (remaining > 0) {
        std::memmove(this->m_buffer.data(), &this->m_buffer[HEADER_SIZE], remaining);
    }
    this->m_bufferSize = remaining;

    return Chunk{this->m_buffer.data(), std::min(remaining, this->m_expectedSize), remaining > 0};
}

ImageProtocolParser::ChunkPlan ImageProtocolParser ::planChunk(std::uint32_t chunkSize) const {
    // Complete is checked before the chunk is written, so a chunk that reaches the expected size does not complete
    if (this->m_bytesReceived >= this->m_expectedSize) {
        return ChunkPlan{true, 0};
    }
    const std::uint32_t remaining = this->m_expectedSize - this->m_bytesReceived;
    return ChunkPlan{false, std::min(chunkSize, remaining)};
}

std::uint8_t ImageProtocolParser ::onChunkWritten(std::uint32_t bytes) {
    this->m_bytesReceived += bytes;

    if (this->m_expectedSize == 0) {
        return 0;
    }

    const std::uint32_t percent =
        static_cast<std::uint32_t>((static_cast<std::uint64_t>(this->m_bytesReceived) * 100) / this->m_expectedSize);

    // At most one milestone per chunk, so a chunk that jumps past several only reports the lowest
    const std::uint8_t milestones[] = {25, 50, 75};
    for (std::uint8_t milestone : milestones) {
        if (percent >= milestone && this->m_lastMilestone < milestone) {
            this->m_lastMilestone = milestone;
            return milestone;
        }
    }
    return 0;
}

void ImageProtocolParser ::onBufferedDataWritten(std::uint32_t bytes) {
    this->m_bytesReceived += bytes;
}

void ImageProtocolParser ::finishTransfer() {
    this->m_receiving = false;
    this->m_bytesReceived = 0;
    this->m_expectedSize = 0;
    this->m_lastMilestone = 0;
}

void ImageProtocolParser ::abortTransfer() {
    this->finishTransfer();
    this->clearBuffer();
}

void ImageProtocolParser ::clearBuffer() {
    this->m_buffer.fill(0);
    this->m_bufferSize = 0;
}

bool ImageProtocolParser ::isComplete() const {
    return this->m_bytesReceived >= this->m_expectedSize;
}

bool ImageProtocolParser ::isReceiving() const {
    return this->m_receiving;
}

std::uint32_t ImageProtocolParser ::bytesReceived() const {
    return this->m_bytesReceived;
}

std::uint32_t ImageProtocolParser ::expectedSize() const {
    return this->m_expectedSize;
}

// ----------------------------------------------------------------------
// Private helper methods
// ----------------------------------------------------------------------

bool ImageProtocolParser ::accumulate(const std::uint8_t* data, std::uint32_t size) {
    if (size > PROTOCOL_BUFFER_SIZE - this->m_bufferSize) {
        return false;
    }
    std::memcpy(&this->m_buffer[this->m_bufferSize], data, size);
    this->m_bufferSize += size;
    return true;
}

ImageProtocolParser::Result ImageProtocolParser ::process() {
    const std::int32_t headerStart = this->find(IMG_START, IMG_START_LEN);

    if (headerStart < 0) {
        if (this->find(PONG, PONG_LEN) >= 0) {
            return Result{Found::PONG, 0};
        }

        // No header: probably text responses, so drop old data when the buffer is getting full
        if (this->m_bufferSize > HALF_FULL) {
            this->keepLast(KEEP_WHEN_HALF_FULL);
        }
        return Result{Found::NOTHING, 0};
    }

    // Discard everything before the header
    if (headerStart > 0) {
        const std::uint32_t remaining = this->m_bufferSize - static_cast<std::uint32_t>(headerStart);
        std::memmove(this->m_buffer.data(), &this->m_buffer[static_cast<std::uint32_t>(headerStart)], remaining);
        this->m_bufferSize = remaining;
    }

    if (this->m_bufferSize < HEADER_SIZE) {
        return Result{Found::NOTHING, 0};  // wait for more data
    }

    if (!this->matches(SIZE_OPEN_OFFSET, SIZE_OPEN, SIZE_OPEN_LEN) ||
        !this->matches(SIZE_CLOSE_OFFSET, SIZE_CLOSE, SIZE_CLOSE_LEN)) {
        return Result{Found::NOTHING, 0};  // invalid header
    }

    std::uint32_t imageSize = 0;
    for (std::uint32_t i = 0; i < SIZE_VALUE_LEN; ++i) {
        imageSize |= static_cast<std::uint32_t>(this->m_buffer[SIZE_VALUE_OFFSET + i]) << (8 * i);
    }

    this->m_receiving = true;
    this->m_bytesReceived = 0;
    this->m_expectedSize = imageSize;
    this->m_lastMilestone = 0;
    return Result{Found::IMAGE_HEADER, imageSize};
}

void ImageProtocolParser ::keepLast(std::uint32_t count) {
    if (this->m_bufferSize > count) {
        std::memmove(this->m_buffer.data(), &this->m_buffer[this->m_bufferSize - count], count);
        this->m_bufferSize = count;
    }
}

std::int32_t ImageProtocolParser ::find(const char* text, std::uint32_t length) const {
    if (this->m_bufferSize < length) {
        return -1;
    }
    for (std::uint32_t i = 0; i <= this->m_bufferSize - length; ++i) {
        if (this->matches(i, text, length)) {
            return static_cast<std::int32_t>(i);
        }
    }
    return -1;
}

bool ImageProtocolParser ::matches(std::uint32_t offset, const char* text, std::uint32_t length) const {
    if (offset + length > this->m_bufferSize) {
        return false;
    }
    return std::memcmp(&this->m_buffer[offset], text, length) == 0;
}

}  // namespace Components

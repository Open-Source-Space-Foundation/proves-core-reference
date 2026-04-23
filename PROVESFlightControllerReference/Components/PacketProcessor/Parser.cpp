// ======================================================================
// \title  Parser.cpp
// \brief  cpp file for Parser implementation class
// ======================================================================

#include "Parser.hpp"

#include <cstdint>
#include <cstring>

namespace Components {
namespace {

constexpr const size_t kSpiSize = 2;
constexpr const size_t kSequenceNumberSize = 4;
constexpr const size_t kSpacePacketHeaderSize = 6;
constexpr const size_t kOpCodeStart = 2;
constexpr const size_t kOpCodeSize = 4;
constexpr const size_t kHeaderSize = kSpiSize + kSequenceNumberSize;
constexpr const size_t kMinPacketSize = kHeaderSize + kSpacePacketHeaderSize + kOpCodeStart + kOpCodeSize;
constexpr const size_t kMinAuthenticatedPacketSize = kMinPacketSize + kHmacSize;

template <typename T>
struct FieldParseResult {
    PacketParser::Status status;
    T value;
};

FieldParseResult<uint32_t> parseSpi(const uint8_t* buffer, const size_t size) {
    // Validate buffer size
    if (!buffer || size < kSpiSize) {
        return {PacketParser::Status::SpiParseError, 0};
    }

    // Extract SPI
    const uint32_t spi = (static_cast<uint32_t>(buffer[0]) << 8) | static_cast<uint32_t>(buffer[1]);

    return {PacketParser::Status::Ok, spi};
}

FieldParseResult<uint32_t> parseSequenceNumber(const uint8_t* buffer, const size_t size) {
    // Validate buffer size
    if (!buffer || size < kHeaderSize) {
        return {PacketParser::Status::SequenceNumberParseError, 0};
    }

    // Extract sequence number
    const uint32_t sequenceNumber = (static_cast<uint32_t>(buffer[2]) << 24) |
                                    (static_cast<uint32_t>(buffer[3]) << 16) | (static_cast<uint32_t>(buffer[4]) << 8) |
                                    static_cast<uint32_t>(buffer[5]);

    return {PacketParser::Status::Ok, sequenceNumber};
}

FieldParseResult<uint32_t> parseOpCode(const uint8_t* buffer, const size_t size) {
    // Validate buffer size
    if (!buffer || size < kMinPacketSize) {
        return {PacketParser::Status::OpCodeParseError, 0};
    }

    // Extract opcode
    const uint8_t* opCodePtr = buffer + kHeaderSize + kSpacePacketHeaderSize + kOpCodeStart;
    const uint32_t opCode = (static_cast<uint32_t>(opCodePtr[0]) << 24) | (static_cast<uint32_t>(opCodePtr[1]) << 16) |
                            (static_cast<uint32_t>(opCodePtr[2]) << 8) | static_cast<uint32_t>(opCodePtr[3]);

    return {PacketParser::Status::Ok, opCode};
}

FieldParseResult<Hmac> parseHmac(const uint8_t* buffer, const size_t size) {
    // Validate buffer size
    if (!buffer || size < kMinAuthenticatedPacketSize) {
        return {PacketParser::Status::HmacParseError, {}};
    }

    // Extract hmac
    Hmac hmac{};
    std::memcpy(hmac.data(), buffer + size - hmac.size(), hmac.size());

    return {PacketParser::Status::Ok, hmac};
}

}  // namespace

PacketParser::Result parsePacket(const uint8_t* buffer, const size_t size) {
    // Parse SPI
    const auto spiResult = parseSpi(buffer, size);
    if (spiResult.status != PacketParser::Status::Ok) {
        return {spiResult.status, {}};
    }

    // Parse sequence number
    const auto sequenceNumberResult = parseSequenceNumber(buffer, size);
    if (sequenceNumberResult.status != PacketParser::Status::Ok) {
        return {sequenceNumberResult.status, {}};
    }

    // Parse OpCode
    const auto opCodeResult = parseOpCode(buffer, size);
    if (opCodeResult.status != PacketParser::Status::Ok) {
        return {opCodeResult.status, {}};
    }

    // Parse HMAC
    const auto hmacResult = parseHmac(buffer, size);
    if (hmacResult.status != PacketParser::Status::Ok) {
        return {hmacResult.status, {}};
    }

    return {PacketParser::Status::Ok,
            Packet{spiResult.value, sequenceNumberResult.value, opCodeResult.value, hmacResult.value}};
}

}  // namespace Components

// ======================================================================
// \title  Parser.cpp
// \brief  cpp file for Parser implementation class
// ======================================================================

#include "Parser.hpp"

#include <cstdint>
#include <cstring>

namespace Components {
namespace {

template <typename T>

//! Struct to hold the result of parsing a field
struct FieldParseResult {
    bool success;  //!< Whether the parsing was successful
    T value;       //!< The parsed value
};

//! Parse the SPI field from the packet buffer
FieldParseResult<uint32_t> parseSpi(const uint8_t* buffer, const size_t size) {
    // Validate buffer size
    if (!buffer || size < Ccsds355_0_B_2::kSpiSize) {
        return {false, 0};
    }

    // Extract SPI
    const uint32_t spi = (static_cast<uint32_t>(buffer[0]) << 8) | static_cast<uint32_t>(buffer[1]);

    return {true, spi};
}

//! Parse the sequence number field from the packet buffer
FieldParseResult<uint32_t> parseSequenceNumber(const uint8_t* buffer, const size_t size) {
    // Validate buffer size
    if (!buffer || size < Ccsds355_0_B_2::kTCSecurityHeaderSize) {
        return {false, 0};
    }

    // Extract sequence number
    const uint32_t sequenceNumber = (static_cast<uint32_t>(buffer[2]) << 24) |
                                    (static_cast<uint32_t>(buffer[3]) << 16) | (static_cast<uint32_t>(buffer[4]) << 8) |
                                    static_cast<uint32_t>(buffer[5]);

    return {true, sequenceNumber};
}

//! Parse the HMAC field from the packet buffer
FieldParseResult<Mac> parseMac(const uint8_t* buffer, const size_t size) {
    // Validate buffer size
    if (!buffer || size < Ccsds355_0_B_2::kMinAuthenticatedPacketSize) {
        return {false, {}};
    }

    // Extract MAC
    Mac mac{};
    std::memcpy(mac.data(), buffer + size - mac.size(), mac.size());

    return {true, mac};
}

}  // namespace

Ccsds355_0_B_2::TcTransferFrame::Parser::Result parse(const uint8_t* buffer, const size_t size) {
    // Parse SPI
    const FieldParseResult<uint32_t> spiResult = parseSpi(buffer, size);
    if (!spiResult.success) {
        return {Ccsds355_0_B_2::TcTransferFrame::Parser::Status::SpiParseError, {}};
    }

    // Parse sequence number
    const FieldParseResult<uint32_t> sequenceNumberResult = parseSequenceNumber(buffer, size);
    if (!sequenceNumberResult.success) {
        return {Ccsds355_0_B_2::TcTransferFrame::Parser::Status::SequenceNumberParseError, {}};
    }

    // Parse MAC
    const FieldParseResult<Mac> macResult = parseMac(buffer, size);
    if (!macResult.success) {
        return {Ccsds355_0_B_2::TcTransferFrame::Parser::Status::MacParseError, {}};
    }

    return {
        Ccsds355_0_B_2::TcTransferFrame::Parser::Status::Ok,
        Ccsds355_0_B_2::TCSecurityHeader{spiResult.value, sequenceNumberResult.value},
        Ccsds355_0_B_2::TCSecurityTrailer{macResult.value},
        Ccsds355_0_B_2::TCFrameData{buffer + Ccsds355_0_B_2::kTCSecurityHeaderSize,
                                    size - Ccsds355_0_B_2::kTCSecurityHeaderSize - Ccsds355_0_B_2::kTCSecurityTrailer}};

}  // namespace Components

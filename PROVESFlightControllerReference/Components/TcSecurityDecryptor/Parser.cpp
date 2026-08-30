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

//! Parse the sequence number field from the packet buffer
FieldParseResult<uint32_t> parseSequenceNumber(const uint8_t* buffer, const size_t size) {
    // Validate buffer size
    if (!buffer || size < Ccsds355_0_B_2::kSequenceNumberSize) {
        return {false, 0};
    }

    // Extract sequence number
    const uint32_t sequenceNumber = (static_cast<uint32_t>(buffer[0]) << 24) |
                                    (static_cast<uint32_t>(buffer[1]) << 16) | (static_cast<uint32_t>(buffer[2]) << 8) |
                                    static_cast<uint32_t>(buffer[3]);

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

namespace Ccsds355_0_B_2 {

TcTransferFrame::Parser::Result parse(const uint8_t* buffer, const size_t size) {
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

    return {TcTransferFrame::Parser::Status::Ok, TCSecurityHeader{sequenceNumberResult.value},
            TCSecurityTrailer{macResult.value},
            TCFrameData{buffer + kSequenceNumberSize, size - kSequenceNumberSize - kTCSecurityTrailer}};
}

}  // namespace Ccsds355_0_B_2
}  // namespace Components

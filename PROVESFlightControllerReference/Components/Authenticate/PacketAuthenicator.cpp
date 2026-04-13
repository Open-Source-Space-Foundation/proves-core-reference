// ======================================================================
// \title  PacketParser.cpp
// \brief  cpp file for PacketParser implementation class
// ======================================================================

#include <cstring>

#include "PacketParser.hpp"

namespace Components {

// ----------------------------------------------------------------------
// Component construction and destruction
// ----------------------------------------------------------------------

PacketParser ::PacketParser() {}

PacketParser ::~PacketParser() {}

// ----------------------------------------------------------------------
// Public helper methods
// ----------------------------------------------------------------------

bool PacketParser ::parsePacket(const uint8_t* dataBuffer, const size_t dataSize, PacketMeta& packetMeta) const {
    // Parse sequence number
    if (!this->parseSequenceNumber(dataBuffer, dataSize, packetMeta.sequenceNumber)) {
        // TODO(nateinaction): Handle failure, maybe pass back custom error?
        return false;
    }

    // Parse opcode
    if (!this->parseOpCode(dataBuffer, dataSize, packetMeta.opCode)) {
        // TODO(nateinaction): Handle failure, maybe pass back custom error?
        return false;
    }

    // Extract hmac
    if (!this->parseHmac(dataBuffer, dataSize, packetMeta.hmacTrailer)) {
        // TODO(nateinaction): Handle failure, maybe pass back custom error?
        return false;
    }

    return true;
}

// ----------------------------------------------------------------------
//  Private helper methods
// ----------------------------------------------------------------------

bool PacketParser ::parseSequenceNumber(const uint8_t* dataBuffer,
                                        const size_t dataSize,
                                        uint32_t& sequenceNumber) const {
    // Validate header size
    if (!dataBuffer || dataSize < kHeaderLength)
        return false;

    // Extract security header
    std::array<std::uint8_t, PacketParser::kHeaderLength> header;
    std::memcpy(header.data(), dataBuffer, header.size());

    // Extract sequence number from header bytes 2-5
    sequenceNumber = (static_cast<uint32_t>(header[2]) << 24) | (static_cast<uint32_t>(header[3]) << 16) |
                     (static_cast<uint32_t>(header[4]) << 8) | static_cast<uint32_t>(header[5]);

    return true;
}

bool PacketParser ::parseOpCode(const uint8_t* dataBuffer, const size_t dataLength, uint32_t& opCode) const {
    constexpr const size_t kOpCodeStart = 2;
    constexpr const size_t kOpCodeLength = 4;

    // Validate buffer size
    if (!dataBuffer || dataLength < kHeaderLength + kOpCodeStart + kOpCodeLength)
        return false;

    // Extract opcode bytes
    const uint8_t* opCodePtr = dataBuffer + kHeaderLength + kOpCodeStart;
    opCode = (static_cast<uint32_t>(opCodePtr[0]) << 24) | (static_cast<uint32_t>(opCodePtr[1]) << 16) |
             (static_cast<uint32_t>(opCodePtr[2]) << 8) | static_cast<uint32_t>(opCodePtr[3]);

    return true;
}

bool PacketParser ::parseHmac(const uint8_t* dataBuffer,
                              const size_t dataSize,
                              std::array<std::uint8_t, PacketParser::kHmacLength>& hmac) const {
    // Validate buffer size
    if (!dataBuffer || dataSize < kHmacLength)
        return false;

    // Extract security trailer
    std::memcpy(hmac.data(), dataBuffer + dataSize - hmac.size(), hmac.size());

    return true;
}

}  // namespace Components

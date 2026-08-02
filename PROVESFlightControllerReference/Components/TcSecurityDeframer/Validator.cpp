// ======================================================================
// \title  Validator.cpp
// \brief  cpp file for packet policy validation helper functions
// ======================================================================

#include "Validator.hpp"

namespace Components {
namespace {

//! Validate the SPI field of the packet
bool spiValid(uint32_t spi) {
    // For now we only support SPI 0, which indicates no additional security processing beyond HMAC
    return spi == 0;
}

//! Validate packet sequence number must be greater than the last accepted sequence number and within the window
bool sequenceNumberValid(uint32_t packetSequenceNumber, uint32_t sequenceNumber, uint32_t sequenceNumberWindow) {
    // Check if the packet sequence number is equal to the current sequence number
    if (packetSequenceNumber == sequenceNumber) {
        return false;
    }

    /*
     * Compute the difference between received and expected sequence numbers using unsigned
     * 32-bit arithmetic. This handles wraparound correctly due to the well-defined behavior
     * of unsigned integer overflow in C++. For example, if expected=0xFFFFFFFE and received=1,
     * then (received - expected) == 3 (modulo 2^32). This is a standard technique for
     * sequence number window validation (see RFC 1982: Serial Number Arithmetic).
     */
    return (packetSequenceNumber - sequenceNumber) <= sequenceNumberWindow;
}

}  // namespace

PacketValidator::Status validatePacket(const Ccsds355_0_B_2::TCSecurityHeader& secHeader,
                                       uint32_t sequenceNumber,
                                       uint32_t sequenceNumberWindow) {
    if (!spiValid(secHeader.spi)) {
        return PacketValidator::Status::SpiInvalid;
    }

    if (!sequenceNumberValid(secHeader.sequenceNumber, sequenceNumber, sequenceNumberWindow)) {
        return PacketValidator::Status::SequenceNumberInvalid;
    }

    return PacketValidator::Status::Valid;
}

}  // namespace Components

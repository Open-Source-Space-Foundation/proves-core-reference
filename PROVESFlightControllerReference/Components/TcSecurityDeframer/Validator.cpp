// ======================================================================
// \title  Validator.cpp
// \brief  cpp file for packet policy validation helper functions
// ======================================================================

#include "Validator.hpp"

namespace Components {
namespace {

// OpCodes that are allowed to bypass authentication
// To determine the OpCode value for a command, search for the command in
// build-artifacts/zephyr/fprime-zephyr-deployment/dict/ReferenceDeploymentTopologyDictionary.json
// then convert the opcode field value from decimal to hexadecimal
static constexpr uint32_t kBypassOpCodes[] = {
    0x01000000,  //!< CdhCore.cmdDisp.CMD_NO_OP
    0x2100B000,  //!< ComCcsdsUart.tcSecurityDeframer.GET_SEQ_NUM
    0x2200B000,  //!< ComCcsdsLora.tcSecurityDeframer.GET_SEQ_NUM
    0x2300B000,  //!< ComCcsdsSBand.tcSecurityDeframer.GET_SEQ_NUM
    0x10065000,  //!< ReferenceDeployment.amateurRadio.TELL_JOKE
};

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

//! Check if the OpCode is in the bypass list
bool opCodeBypassAllowed(uint32_t opCode) {
    // Check if the OpCode is in the bypass list
    for (size_t i = 0; i < (sizeof(kBypassOpCodes) / sizeof(kBypassOpCodes[0])); i++) {
        if (opCode == kBypassOpCodes[i]) {
            return true;
        }
    }

    return false;
}

}  // namespace

PacketValidator::Status validatePacket(const Packet& packet, uint32_t sequenceNumber, uint32_t sequenceNumberWindow) {
    // Check if the packet can bypass authentication based on its OpCode
    if (opCodeBypassAllowed(packet.opCode)) {
        return PacketValidator::Status::Bypass;
    }

    // Validate SPI
    if (!spiValid(packet.spi)) {
        return PacketValidator::Status::SpiInvalid;
    }

    // Validate sequence number
    if (!sequenceNumberValid(packet.sequenceNumber, sequenceNumber, sequenceNumberWindow)) {
        return PacketValidator::Status::SequenceNumberInvalid;
    }

    return PacketValidator::Status::Valid;
}

}  // namespace Components

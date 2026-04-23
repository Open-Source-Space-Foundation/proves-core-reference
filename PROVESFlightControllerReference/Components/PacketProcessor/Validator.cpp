// ======================================================================
// \title  Validator.cpp
// \brief  cpp file for packet policy validation helper functions
// ======================================================================

#include "Validator.hpp"

namespace Components {
namespace {

// TODO(nateinaction): Re-add comments explaining the rationale behind each of these validation checks
bool spiValid(uint32_t spi) {
    return spi == 0;
}

bool sequenceNumberInWindow(uint32_t sequenceNumber, uint32_t expectedSequenceNumber, uint32_t sequenceNumberWindow) {
    return (sequenceNumber - expectedSequenceNumber) <= sequenceNumberWindow;
}

bool opCodeBypassAllowed(uint32_t opCode) {
    static constexpr uint32_t kBypassOpCodes[] = {
        0x01000000,  // no op
        0x2200B000,  // get sequence number
        0x10065000,  // amateurRadio.TELL_JOKE
    };

    for (size_t i = 0; i < (sizeof(kBypassOpCodes) / sizeof(kBypassOpCodes[0])); i++) {
        if (opCode == kBypassOpCodes[i]) {
            return true;
        }
    }

    return false;
}

}  // namespace

PacketValidator::Status validatePacket(const Packet& packet,
                                       uint32_t expectedSequenceNumber,
                                       uint32_t sequenceNumberWindow) {
    if (!spiValid(packet.spi)) {
        return PacketValidator::Status::SpiInvalid;
    }

    if (!sequenceNumberInWindow(packet.sequenceNumber, expectedSequenceNumber, sequenceNumberWindow)) {
        return PacketValidator::Status::SequenceNumberOutOfWindow;
    }

    if (opCodeBypassAllowed(packet.opCode)) {
        return PacketValidator::Status::Bypass;
    }

    return PacketValidator::Status::Valid;
}

}  // namespace Components

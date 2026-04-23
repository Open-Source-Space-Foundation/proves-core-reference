// ======================================================================
// \title  Validator.hpp
// \brief  hpp file for packet policy validation helper functions
// ======================================================================

#pragma once

#include <cstddef>
#include <cstdint>

#include "Types.hpp"

namespace Components {
namespace PacketValidator {

//! Status of validation attempt
//! Must match the ValidatorStatus enum in the .fpp file
enum class Status {
    Valid,
    Bypass,
    SpiInvalid,
    SequenceNumberOutOfWindow,
};

}  // namespace PacketValidator

PacketValidator::Status validatePacket(
    const Packet& packet,             //!< The Parsed packet to validate
    uint32_t expectedSequenceNumber,  //!< The expected sequence number for the packet
    uint32_t sequenceNumberWindow     //!< The acceptable sequence number window
);

}  // namespace Components

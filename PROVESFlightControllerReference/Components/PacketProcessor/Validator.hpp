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
    Valid,                      //!< The packet is valid and passes all checks
    Bypass,                     //!< The packet is allowed to bypass authentication based on its OpCode
    SpiInvalid,                 //!< The packet SPI field is invalid
    SequenceNumberOutOfWindow,  //!< The packet sequence number is outside the acceptable window
};

}  // namespace PacketValidator

//! Validate the packet against ruleset
PacketValidator::Status validatePacket(
    const Packet& packet,             //!< The Parsed packet to validate
    uint32_t expectedSequenceNumber,  //!< The expected sequence number for the packet
    uint32_t sequenceNumberWindow     //!< The acceptable sequence number window
);

}  // namespace Components

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
    Valid,                  //!< The packet is valid and passes all checks
    Bypass,                 //!< The packet is allowed to bypass authentication based on its OpCode
    SpiInvalid,             //!< The packet SPI field is invalid
    SequenceNumberInvalid,  //!< The packet sequence number is outside the acceptable window
};

}  // namespace PacketValidator

//! Validate the packet against ruleset
PacketValidator::Status validatePacket(
    const Ccsds355_0_B_2::TCSecurityHeader& secHeader,  //!< The parsed security header
    uint32_t sequenceNumber,                            //!< The current sequence number
    uint32_t sequenceNumberWindow                       //!< The acceptable sequence number window
);

}  // namespace Components

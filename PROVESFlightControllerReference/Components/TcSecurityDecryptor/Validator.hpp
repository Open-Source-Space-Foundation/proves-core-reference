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
enum class Status {
    Valid,                  //!< The packet is valid and passes all checks
    SpiInvalid,             //!< The security association index is invalid
    SequenceNumberInvalid,  //!< The packet sequence number is outside the acceptable window
};

}  // namespace PacketValidator

//! Validate the frame against ruleset. The security association index is supplied by the caller
//! (sourced from the upstream Svc.Ccsds.CcsdsSdlsDeframer) rather than parsed from the frame.
PacketValidator::Status validateFrame(uint32_t saIndex,               //!< The security association index
                                      uint32_t packetSequenceNumber,  //!< The sequence number parsed from the frame
                                      uint32_t sequenceNumber,        //!< The current sequence number
                                      uint32_t sequenceNumberWindow   //!< The acceptable sequence number window
);

}  // namespace Components

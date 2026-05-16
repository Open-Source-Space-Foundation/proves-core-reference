// ======================================================================
// \title  Bypasser.hpp
// \brief  hpp file for packet policy validation helper functions
// ======================================================================

#pragma once

#include <cstddef>
#include <cstdint>

namespace Components {
namespace PacketBypasser {

//! Status of bypass attempt
//! Must match the BypasserStatus enum in the .fpp file
enum class Status {
    RequiresAuthentication,  //!< The packet requires authentication
    BypassAllowed,           //!< The packet is allowed to bypass authentication
    OpCodeParseError,        //!< OpCode could not be parsed from packet
};

}  // namespace PacketBypasser

//! Determine if the packet can bypass authentication
PacketBypasser::Status bypassPacket(const uint8_t* buffer,  //!< The transfer frame buffer
                                    const size_t size       //!< The transfer frame size
);

}  // namespace Components

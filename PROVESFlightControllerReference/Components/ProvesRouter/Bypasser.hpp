// ======================================================================
// \title  Bypasser.hpp
// \brief  hpp file for packet policy validation helper functions
// ======================================================================

#pragma once

#include <cstddef>
#include <cstdint>

namespace Components {
namespace PacketBypasser {

//! Determine if the packet can bypass authentication
bool bypassPacket(const uint8_t* buffer,  //!< The transfer frame buffer
                  const size_t size       //!< The transfer frame size
);

}  // namespace Components

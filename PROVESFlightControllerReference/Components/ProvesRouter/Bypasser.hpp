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
bool bypassPacket(const uint8_t* buffer,  //!< The packet buffer
                  const size_t size       //!< The packet size
);

}  // namespace PacketBypasser
}  // namespace Components

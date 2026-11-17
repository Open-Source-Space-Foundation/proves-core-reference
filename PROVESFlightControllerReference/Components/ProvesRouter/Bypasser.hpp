// ======================================================================
// \title  Bypasser.hpp
// \brief  hpp file for packet policy validation helper functions
// ======================================================================

#pragma once

#include <cstddef>
#include <cstdint>

namespace Components {
namespace PacketBypasser {

//! APID of command packets; must equal ComCfg::Apid::FW_PACKET_COMMAND (checked in ProvesRouter.cpp)
constexpr uint16_t kCommandApid = 0x0000;

//! Determine if the packet can bypass authentication
bool bypassPacket(const uint16_t apid,    //!< The packet APID
                  const uint8_t* buffer,  //!< The packet buffer
                  const size_t size       //!< The packet size
);

}  // namespace PacketBypasser
}  // namespace Components

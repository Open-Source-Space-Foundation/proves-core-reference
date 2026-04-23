// ======================================================================
// \title  Types.cpp
// \brief  hpp file for to define types used by PacketProcessor component
// ======================================================================

#pragma once

#include <array>
#include <cstdint>

#include "Const.hpp"

namespace Components {

using Hmac = std::array<uint8_t, Ccsds355_0_B_2_Cmac::kSecurityTrailerSize>;

//! Packet structure to hold parsed values
struct Packet {
    uint32_t spi;             //!< The Security Parameter Index of the packet
    uint32_t sequenceNumber;  //!< The sequence number of the packet
    uint32_t opCode;          //!< The OpCode of the command contained in the packet
    Hmac hmac;                //!< The HMAC extracted from the packet
};

}  // namespace Components

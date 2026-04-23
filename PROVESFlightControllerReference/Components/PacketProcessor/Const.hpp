// ======================================================================
// \title  Const.cpp
// \brief  hpp file for to define constants used by PacketProcessor component
// ======================================================================

#pragma once

#include <cstddef>

namespace Components {

constexpr size_t kHeaderSize = 6;  //!< The size of the packet header in bytes
constexpr size_t kHmacSize = 16;   //!< The size of the HMAC in bytes

}  // namespace Components

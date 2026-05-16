// ======================================================================
// \title  Const.cpp
// \brief  hpp file for to define constants used by SecurityRouter component
// ======================================================================

#pragma once

#include <cstddef>

namespace Components {

namespace SpacePacket {

constexpr const size_t kHeaderSize = 6;    //!< The space packet header size in bytes
constexpr const size_t kOpCodeOffset = 2;  //!< Offset into the space packet data field where the OpCode begins
constexpr const size_t kOpCodeSize = 4;    //!< The size of the OpCode field in bytes

}  // namespace SpacePacket

}  // namespace Components

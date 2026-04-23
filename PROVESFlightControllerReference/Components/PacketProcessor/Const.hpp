// ======================================================================
// \title  Const.cpp
// \brief  hpp file for to define constants used by PacketProcessor component
// ======================================================================

#pragma once

#include <cstddef>

namespace Components {

// CCSDS 355.0-B-2 CMAC mode
namespace Ccsds355_0_B_2_Cmac {

constexpr const size_t kSpiSize = 2;                //!< The size of the Security Parameter Index field in bytes
constexpr const size_t kSequenceNumberSize = 4;     //!< The size of the sequence number field in bytes
constexpr const size_t kSpacePacketHeaderSize = 6;  //!< The size of the CCSDS space packet primary header in bytes
constexpr const size_t kOpCodeStart = 2;            //!< Offset into the space packet data field where the OpCode begins
constexpr const size_t kOpCodeSize = 4;             //!< The size of the OpCode field in bytes
constexpr const size_t kSecurityTrailerSize = 16;   //!< The size of the HMAC security trailer in bytes

constexpr const size_t kSecurityHeaderSize = kSpiSize + kSequenceNumberSize;  //!< The size of the security header
constexpr const size_t kMinPacketSize =
    kSecurityHeaderSize + kSpacePacketHeaderSize + kOpCodeStart + kOpCodeSize;  //!< Minimum packet size without HMAC
constexpr const size_t kMinAuthenticatedPacketSize =
    kMinPacketSize + kSecurityTrailerSize;  //!< Minimum packet size including HMAC

}  // namespace Ccsds355_0_B_2_Cmac

}  // namespace Components

// ======================================================================
// \title  Const.cpp
// \brief  hpp file for to define constants used by TcSecurityDeframer component
// ======================================================================

#pragma once

#include <cstddef>

namespace Components {

//! CCSDS 355.0-B-2
//! https://ccsds.org/Pubs/355x0b2.pdf
namespace Ccsds355_0_B_2 {

//! Telecommand packet structure

//! CCSDS 355.0-B-2 Section E2.2
constexpr const size_t kSpiSize = 2;             //!< The size of the Security Parameter Index field in bytes
constexpr const size_t kSequenceNumberSize = 4;  //!< The size of the sequence number field in bytes
constexpr const size_t kTCSecurityHeaderSize =
    kSpiSize + kSequenceNumberSize;               //!< The telecommand security header size in bytes
constexpr const size_t kTCPrimaryHeaderSize = 5;  //!< TC Primary Header is always 5 octets (CCSDS 232.0-B-4)

//! CCSDS 355.0-B-2 Section E2.3
constexpr const size_t kTCSecurityTrailer = 16;  //!< The telecommand security trailer size in bytes

//! Helpers
constexpr const size_t kMinAuthenticatedPacketSize =
    kTCSecurityHeaderSize + kTCSecurityTrailer;  //!< Minimum packet size

}  // namespace Ccsds355_0_B_2

}  // namespace Components

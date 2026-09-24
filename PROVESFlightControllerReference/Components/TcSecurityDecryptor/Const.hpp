// ======================================================================
// \title  Const.cpp
// \brief  hpp file for to define constants used by TcSecurityDecryptor component
// ======================================================================

#pragma once

#include <cstddef>

namespace Components {

//! CCSDS 355.0-B-2
//! https://ccsds.org/Pubs/355x0b2.pdf
namespace Ccsds355_0_B_2 {

//! Telecommand packet structure

//! CCSDS 355.0-B-2 Section E2.2
//! The Security Parameter Index is stripped by the upstream Svc.Ccsds.CcsdsSdlsDeframer before
//! this component sees the frame; kSpiSize is kept only for the MAC-prefix fallback in Authenticator.
constexpr const size_t kSpiSize = 2;             //!< The size of the Security Parameter Index field in bytes
constexpr const size_t kSequenceNumberSize = 4;  //!< The size of the sequence number field in bytes

//! CCSDS 355.0-B-2 Section E2.3
constexpr const size_t kTCSecurityTrailer = 16;  //!< The telecommand security trailer size in bytes

//! Helpers
constexpr const size_t kMinAuthenticatedPacketSize = kSequenceNumberSize + kTCSecurityTrailer;  //!< Minimum packet size

}  // namespace Ccsds355_0_B_2

}  // namespace Components

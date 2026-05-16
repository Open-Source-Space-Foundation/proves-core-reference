// ======================================================================
// \title  Types.cpp
// \brief  hpp file for to define types used by TcSecurityDeframer component
// ======================================================================

#pragma once

#include <array>
#include <cstdint>

#include "Const.hpp"

namespace Components {

using Mac = std::array<uint8_t, Ccsds355_0_B_2::kTCSecurityTrailer>;  //!< The MAC field 16 octets in length

//! CCSDS 355.0-B-2
//! https://ccsds.org/Pubs/355x0b2.pdf
namespace Ccsds355_0_B_2 {

//! Describes the frame security header format for a Telecommand (TC) Transfer Frame
struct TCSecurityHeader {
    uint32_t spi;             //!< The Security Parameter Index of the packet
    uint32_t sequenceNumber;  //!< The sequence number of the packet
};

//! Frame data for a Telecommand (TC) Transfer Frame
struct TCFrameData {
    const uint8_t* data;  //!< Pointer to the start of the frame data
    size_t size;          //!< The size of the frame data in bytes
};

//! Describes the frame security trailer format for a Telecommand (TC) Transfer Frame
struct TCSecurityTrailer {
    Mac mac;  //!< The message authentication code (MAC) field of the packet
};

namespace Verification {

//! Verification return
//! CCSDS 355.0-B-2 Section 3.3.3.3
struct Result {
    Status status;                        //!< Whether the Transfer Frame passed verification
    StatusCode statusCode;                //!< The status code indicates the type of verification failure
    Ccsds355_0_B_2::TCFrameData payload;  //!< The portion of the Transfer Frame corresponding to the ProcessSecurity Payload, starting at the first octet following the Security Header and ending at the last octet of the Transfer Frame Data Field
};

//! Verification status
//! CCSDS 355.0-B-2 Section 3.3.3.1
enum class Status {
    Ok,      //!< No failures detected
    Failed,  //!< Failure detected
};

//! Verification status codes
//! CCSDS 355.0-B-2 Section 3.3.3.2
//! Must match the VerificationStatusCode enum in the .fpp file
enum class StatusCode {
    Success,               //!< No failures detected
    SpiInvalid,            //!< SPI was invalid
    MacFailed,             //!< MAC verification failed
    SequenceNumberFailed,  //!< Anti-replay sequence number verification failed
    PaddingError,          //!< Padding error in the packet
};

}  // namespace Verification

}  // namespace Ccsds355_0_B_2

}  // namespace Components

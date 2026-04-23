// ======================================================================
// \title  Authenticator.hpp
// \brief  hpp file for packet HMAC authentication functions
// ======================================================================

#pragma once

#include <psa/crypto.h>

#include <array>
#include <cstddef>
#include <cstdint>

#include "Const.hpp"
#include "Types.hpp"

namespace Components {
namespace PacketAuthenticator {

//! Status of authentication attempt
//! Must match the AuthenticatorStatus enum in the .fpp file
enum class Status {
    Authenticated,    //!< Packet is authenticated
    InitError,        //!< There was an error initializing the authentication process
    ParseKeyError,    //!< There was an error parsing the key from storage
    ImportKeyError,   //!< There was an error importing the authentication key
    VerifyError,      //!< The packet HMAC did not match the expected value
    DestroyKeyError,  //!< There was an error destroying the authentication key
};

//! Result of authentication attempt
struct Result {
    Status status;           //!< The status of the authentication attempt
    psa_status_t psaStatus;  //!< The status code returned by the PSA crypto functions
};

}  // namespace PacketAuthenticator

//! authenticate checks the validity of the packet HMAC
PacketAuthenticator::Result authenticatePacket(
    const uint8_t* buffer,  //!< The packet data buffer
    size_t size,            //!< The size of the data buffer
    const Hmac& hmac        //!< The HMAC extracted from the packet to validate against
);

}  // namespace Components

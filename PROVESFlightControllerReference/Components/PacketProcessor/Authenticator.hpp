// ======================================================================
// \title  Authenticator.hpp
// \brief  hpp file for packet HMAC authentication functions
// ======================================================================

#pragma once

#include <cstddef>
#include <cstdint>

#include "Types.hpp"

namespace Components {
namespace PacketAuthenticator {

//! Status of the key import attempt
enum class KeyImportStatus {
    Success,         //!< Key was successfully imported
    InitError,       //!< There was an error initializing the authentication process
    ParseKeyError,   //!< There was an error parsing the key from storage
    ImportKeyError,  //!< There was an error importing the authentication key
};

struct KeyImportResult {
    KeyImportStatus status;  //!< The status of the key import attempt
    int32_t psaStatus;       //!< The status code returned by the PSA crypto functions
};

//! Status of authentication attempt
//! Must match the AuthenticatorStatus enum in the .fpp file
enum class AuthenticationStatus {
    Authenticated,  //!< Packet is authenticated
    VerifyError,    //!< The packet HMAC did not match the expected value
};

//! Result of authentication attempt
struct AuthenticationResult {
    AuthenticationStatus status;  //!< The status of the authentication attempt
    int32_t psaStatus;            //!< The status code returned by the PSA crypto functions
};

}  // namespace PacketAuthenticator

//! Import an HMAC key into PSA for message verification.
PacketAuthenticator::KeyImportResult importHmacKey(const char* key,  //!< The hex-encoded authentication key to import
                                                   uint32_t& keyId   //!< The key ID to use for the imported key
);

//! Check the validity of the packet HMAC
PacketAuthenticator::AuthenticationResult authenticatePacket(
    const uint8_t* buffer,  //!< The packet data buffer
    size_t size,            //!< The size of the data buffer
    const Hmac& hmac,       //!< The HMAC extracted from the packet to validate against
    uint32_t& keyId         //!< The hex-encoded authentication key to use for validation
);

}  // namespace Components

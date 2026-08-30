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
    ImportKeyError,  //!< There was an error importing the authentication key
};

//! Mirror of PSA_SUCCESS, so callers can check a psaStatus without including <psa/crypto.h>
//! (this header is part of the pure-C++ layer covered by the gtest unit tests). Authenticator.cpp
//! static_asserts that it still matches.
constexpr int32_t kPsaSuccess = 0;

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

//! Parse a 32-character hex string (16 bytes) into a byte array.
//! Returns true on success and fills `keyBytes` with the parsed bytes.
bool parseHexKey(const char* key,                                         //!< The hex-encoded key string to parse
                 uint8_t (&keyBytes)[Ccsds355_0_B_2::kTCSecurityTrailer]  //!< The parsed key bytes
);

//! Import a raw 128-bit HMAC key into PSA for message verification.
PacketAuthenticator::KeyImportResult importHmacKeyBytes(
    const uint8_t (&keyBytes)[Ccsds355_0_B_2::kTCSecurityTrailer],  //!< The raw key bytes to import
    uint32_t& keyId                                                 //!< The key ID to use for the imported key
);

//! Destroy a previously-imported PSA key. Used to release the old key on rotation.
//! Returns the PSA status: a failed destroy leaves the old key usable in PSA, which the caller
//! must not silently treat as a released slot.
int32_t destroyHmacKey(uint32_t keyId  //!< The PSA key ID to destroy
);

//! Check the validity of the packet HMAC
PacketAuthenticator::AuthenticationResult authenticatePacket(
    const uint8_t* buffer,  //!< The packet data buffer
    size_t size,            //!< The size of the data buffer
    const Mac& hmac,        //!< The HMAC extracted from the packet to validate against
    uint32_t keyId          //!< The PSA key ID to use for validation
);

}  // namespace Components

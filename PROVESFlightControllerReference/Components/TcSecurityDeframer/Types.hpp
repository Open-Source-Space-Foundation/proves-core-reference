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

constexpr size_t kMaxActiveKeys = 2;  //!< Max simultaneously active auth keys (mirrors AuthKeyStore::SIZE)

//! A single active-key SPI slot. Mirrors the FPP-generated AuthKeySlot's `valid`/`spi` fields
//! without depending on the FPP/F Prime type, so Validator.cpp can stay pure C++.
struct ActiveSpiSlot {
    bool valid;    //!< Whether this slot holds an active key
    uint32_t spi;  //!< The SPI associated with the active key
};

using ActiveSpiSlots = std::array<ActiveSpiSlot, kMaxActiveKeys>;  //!< The set of currently active SPIs

//! Key-store admission policy.
//!
//! The decision "may this board be provisioned / may its key store be rewritten" is expressed here
//! as pure C++ over probe results, deliberately away from any Os:: type, so it can be exercised
//! directly by the host gtest suite across the full status matrix.
//!
//! Why it cannot be written directly against Os::File::Status: on the Zephyr target a *genuinely
//! missing* file does not report Os::File::DOESNT_EXIST. ZephyrFile::open discards fs_open's errno
//! and returns OTHER_ERROR for every failure, so DOESNT_EXIST is unreachable on flight hardware
//! while it is the normal result on the POSIX host. Any gate written as
//! `status != OP_OK && status != DOESNT_EXIST -> refuse` therefore passes on the host and refuses
//! on the target, which is exactly how a brand-new board became unprovisionable. The read status
//! alone is not a usable signal; the caller must probe the filesystem for the answer.
namespace KeyStore {

//! Whether the filesystem holding the key store is demonstrably mounted.
enum class MountProbe {
    Live,     //!< The mount point answered a statvfs, so the filesystem is up
    Unknown,  //!< The mount point could not be interrogated: not mounted, not ready, or erroring
};

//! What is demonstrably true about the key store file itself.
enum class StoreProbe {
    Present,     //!< The store was read back in full; its contents are known
    Absent,      //!< The store was positively confirmed not to exist (stat says so)
    Unreadable,  //!< The store may or may not exist, and its contents are unknown
};

//! True when the on-flash key store's contents are known with certainty.
//!
//! Note the asymmetry: `Present` needs no mount probe, because a successful full read is itself
//! proof the filesystem served the file. `Absent` does need one, because on Zephyr fs_stat reports
//! -ENOENT both for "file missing from a mounted filesystem" and for "no such mount point"
//! (subsys/fs/fs.c fs_get_mnt_point). Treating the latter as emptiness would let an attacker who
//! can keep /keys from mounting provision their own key over a board that still holds a valid one.
constexpr bool storeStateIsKnown(MountProbe mount, StoreProbe store) {
    return (store == StoreProbe::Present) || (store == StoreProbe::Absent && mount == MountProbe::Live);
}

//! True when trust-on-first-use provisioning (PROVISION_KEY) may proceed.
//!
//! PROVISION_KEY is bypass-allowlisted, i.e. reachable unauthenticated over RF, so it is only safe
//! while the store is *proven* empty. Proven means: we know what is on flash (storeStateIsKnown)
//! and it holds no active key. An unreadable store is never provisionable.
constexpr bool storeIsProvisionable(MountProbe mount, StoreProbe store, uint8_t activeKeyCount) {
    return storeStateIsKnown(mount, store) && activeKeyCount == 0;
}

}  // namespace KeyStore

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

//! Verification status — CCSDS 355.0-B-2 Section 3.3.3.1
enum class Status {
    Ok,      //!< No failures detected
    Failed,  //!< Failure detected
};

//! Verification status codes — CCSDS 355.0-B-2 Section 3.3.3.2
//! Must match the VerificationStatusCode enum in the .fpp file
enum class StatusCode {
    Success = 0,               //!< No failures detected
    SpiInvalid = 1,            //!< SPI was invalid
    MacFailed = 2,             //!< MAC verification failed
    SequenceNumberFailed = 3,  //!< Anti-replay sequence number verification failed
    PaddingError = 4,          //!< Padding error in the packet
};

//! Verification return — CCSDS 355.0-B-2 Section 3.3.3.3
struct Result {
    Status status;          //!< Whether the Transfer Frame passed verification
    StatusCode statusCode;  //!< The type of verification failure
};

}  // namespace Verification

}  // namespace Ccsds355_0_B_2

}  // namespace Components

// ======================================================================
// \title  test_TcSecurityDeframer_KeyStorePolicy.cpp
// \brief  Unit tests for the pure key-store admission policy (Components::KeyStore)
//
// These tests guard a bug class that a component-level host test structurally cannot catch: the
// original defect only manifested on the Zephyr target, because ZephyrFile::open collapses every
// fs_open errno into Os::File::OTHER_ERROR, while the POSIX host maps ENOENT to
// Os::File::DOESNT_EXIST. A gate written as "refuse unless OP_OK or DOESNT_EXIST" therefore passed
// every host test and bricked a factory-fresh board.
//
// The fix pushes the decision behind a pure predicate over *probe results* rather than over a read
// status, so the divergence has nowhere to hide: there is no "missing file" status input to get
// wrong, and the matrix below is exhaustive over the predicate's entire domain.
// ======================================================================

#include <gtest/gtest.h>

#include "PROVESFlightControllerReference/Components/TcSecurityDeframer/Types.hpp"

using namespace Components;
using KeyStore::MountProbe;
using KeyStore::storeIsProvisionable;
using KeyStore::StoreProbe;
using KeyStore::storeStateIsKnown;

namespace {

constexpr MountProbe kMounts[] = {MountProbe::Live, MountProbe::Unknown};
constexpr StoreProbe kStores[] = {StoreProbe::Present, StoreProbe::Absent, StoreProbe::Unreadable};

}  // namespace

// ----------------------------------------------------------------------
// The bootstrap path: a keyless board must be able to accept its first key
// ----------------------------------------------------------------------

//! The C1 regression itself: a brand-new board (/keys freshly formatted, store file never written)
//! must be provisionable, or it is permanently keyless and unreachable by command.
TEST(KeyStorePolicy, ColdBoardOnLiveMountIsProvisionable) {
    EXPECT_TRUE(storeIsProvisionable(MountProbe::Live, StoreProbe::Absent, 0));
}

//! A store file that exists and reads back cleanly but holds no valid slot is equally proven empty.
TEST(KeyStorePolicy, ReadableButEmptyStoreIsProvisionable) {
    EXPECT_TRUE(storeIsProvisionable(MountProbe::Live, StoreProbe::Present, 0));
    // A successful read is its own proof the filesystem is up, so it does not need a mount probe.
    EXPECT_TRUE(storeIsProvisionable(MountProbe::Unknown, StoreProbe::Present, 0));
}

// ----------------------------------------------------------------------
// The security property: trust-on-first-use may never overwrite a live key
// ----------------------------------------------------------------------

//! PROVISION_KEY is bypass-allowlisted (unauthenticated, reachable over RF), so once any key is on
//! flash it must be refused; rotation has to go through the authenticated ADD_KEY/REMOVE_KEY.
TEST(KeyStorePolicy, ProvisionedBoardIsNotProvisionable) {
    for (uint8_t count = 1; count <= kMaxActiveKeys; count++) {
        EXPECT_FALSE(storeIsProvisionable(MountProbe::Live, StoreProbe::Present, count))
            << "count=" << static_cast<int>(count);
    }
}

//! The core hardening property: if we cannot tell what is on flash, we must not write to it. An
//! attacker who can induce a read failure (glitching, a corrupt record, a wedged littlefs) must not
//! thereby be able to install their own key over a valid one.
TEST(KeyStorePolicy, UnreadableStoreIsNeverProvisionable) {
    for (MountProbe mount : kMounts) {
        for (uint8_t count = 0; count <= kMaxActiveKeys; count++) {
            EXPECT_FALSE(storeIsProvisionable(mount, StoreProbe::Unreadable, count));
            EXPECT_FALSE(storeStateIsKnown(mount, StoreProbe::Unreadable));
        }
    }
}

//! The trap in the naive fix. On Zephyr, fs_open/fs_stat return -ENOENT both for "file missing from
//! a mounted filesystem" and for "that mount point does not exist" (subsys/fs/fs.c
//! fs_get_mnt_point). So mapping ENOENT straight to "absent, therefore empty, therefore
//! provisionable" would let anyone who can keep /keys from mounting take over a board that still
//! holds a valid key. Absence only counts when the mount independently answered.
TEST(KeyStorePolicy, AbsentFileWithoutLiveMountIsNeverProvisionable) {
    EXPECT_FALSE(storeIsProvisionable(MountProbe::Unknown, StoreProbe::Absent, 0));
    EXPECT_FALSE(storeStateIsKnown(MountProbe::Unknown, StoreProbe::Absent));
}

// ----------------------------------------------------------------------
// Exhaustive matrix
// ----------------------------------------------------------------------

//! Pin the entire domain, so any future widening of the predicate has to be a deliberate edit here.
TEST(KeyStorePolicy, ExhaustiveMatrix) {
    for (MountProbe mount : kMounts) {
        for (StoreProbe store : kStores) {
            const bool expectKnown =
                (store == StoreProbe::Present) || (store == StoreProbe::Absent && mount == MountProbe::Live);
            EXPECT_EQ(expectKnown, storeStateIsKnown(mount, store));

            for (uint8_t count = 0; count <= kMaxActiveKeys; count++) {
                EXPECT_EQ(expectKnown && count == 0, storeIsProvisionable(mount, store, count));
            }
        }
    }
}

// ----------------------------------------------------------------------
// Host/target divergence documentation
// ----------------------------------------------------------------------

//! Mirrors of the Os::File::Status values a key-store read can produce, kept as plain ints so this
//! pure test needs no Os:: dependency. The point of this test is the *classification* step that
//! TcSecurityDeframer::probeKeyStore performs, and specifically that the classification is not
//! allowed to depend on the read status.
enum class ReadStatus { OpOk, DoesntExist, OtherError, BadSize };

//! What the old (buggy) gate concluded, purely from the read status.
bool legacyGateAllowed(ReadStatus status) {
    return status == ReadStatus::OpOk || status == ReadStatus::DoesntExist;
}

//! This is the whole bug in one assertion. On the POSIX host a missing file reads back as
//! DOESNT_EXIST and the legacy gate lets provisioning through; on the Zephyr target the identical
//! situation reads back as OTHER_ERROR and the legacy gate refuses. Same board state, opposite
//! answer, decided by which Os layer you happened to compile against. Any future gate that consumes
//! a raw read status reintroduces this.
TEST(KeyStorePolicy, LegacyStatusGateDivergesBetweenHostAndTarget) {
    // The same physical condition - key store file genuinely absent:
    const ReadStatus onHost = ReadStatus::DoesntExist;   // POSIX open() sets ENOENT
    const ReadStatus onTarget = ReadStatus::OtherError;  // ZephyrFile::open discards fs_open's errno

    EXPECT_TRUE(legacyGateAllowed(onHost));
    EXPECT_FALSE(legacyGateAllowed(onTarget));
    EXPECT_NE(legacyGateAllowed(onHost), legacyGateAllowed(onTarget))
        << "A missing key store must not produce different decisions on host and target";

    // The replacement predicate takes probe results, not a status, so both platforms feed it the
    // same StoreProbe::Absent + MountProbe::Live and get the same answer.
    EXPECT_TRUE(storeIsProvisionable(MountProbe::Live, StoreProbe::Absent, 0));

    // And OTHER_ERROR that is *not* a missing file (a truncated record, a wedged FS) still lands on
    // Unreadable, which is refused - the hardening property survives.
    EXPECT_FALSE(storeIsProvisionable(MountProbe::Live, StoreProbe::Unreadable, 0));
    EXPECT_FALSE(storeIsProvisionable(MountProbe::Live, StoreProbe::Unreadable, 1));
}

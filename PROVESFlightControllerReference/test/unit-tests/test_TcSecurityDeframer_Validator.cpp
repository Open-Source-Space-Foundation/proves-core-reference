#include <gtest/gtest.h>

#include "PROVESFlightControllerReference/Components/TcSecurityDeframer/Validator.hpp"

using namespace Components;
using Header = Ccsds355_0_B_2::TCSecurityHeader;

TEST(PacketValidatorTest, ValidPacket) {
    Header h{0u, 11u};  // spi 0 valid, seq 11 within window of expected 10
    auto res = validatePacket(h, 10u, 5u);
    EXPECT_EQ(res, PacketValidator::Status::Valid);
}

TEST(PacketValidatorTest, SpiInvalid) {
    Header h{1u, 11u};  // non-zero SPI invalid
    auto res = validatePacket(h, 10u, 5u);
    EXPECT_EQ(res, PacketValidator::Status::SpiInvalid);
}

TEST(PacketValidatorTest, SequenceNumberInvalid) {
    Header h{0u, 20u};  // sequence 20, expected 10, window 5 -> out
    auto res = validatePacket(h, 10u, 5u);
    EXPECT_EQ(res, PacketValidator::Status::SequenceNumberInvalid);
}

TEST(PacketValidatorTest, SequenceNumberReplayRejected) {
    Header h{0u, 9u};  // below last accepted -> replay
    auto res = validatePacket(h, 10u, 5u);
    EXPECT_EQ(res, PacketValidator::Status::SequenceNumberInvalid);
}

TEST(PacketValidatorTest, SequenceNumberWrapWithinWindow) {
    // expected near U32 max, sequence wraps to small value within window
    const uint32_t expected = 0xFFFFFFFEu;  // -2
    const uint32_t seq = 1u;                // wrapped value
    // distance = seq - expected = 1 - 0xFFFFFFFE = 3 (mod 2^32)
    Header h{0u, seq};

    auto res = validatePacket(h, expected, 5u);
    EXPECT_EQ(res, PacketValidator::Status::Valid);
}

TEST(PacketValidatorTest, SequenceNumberWrapOutOfWindow) {
    // expected near U32 max, sequence wraps to a value outside window
    const uint32_t expected = 0xFFFFFFF0u;  // -16
    const uint32_t seq = 10u;               // wrapped value
    // distance = 10 - 0xFFFFFFF0 = 26 (mod 2^32)
    Header h{0u, seq};

    auto res = validatePacket(h, expected, 5u);
    EXPECT_EQ(res, PacketValidator::Status::SequenceNumberInvalid);
}

TEST(PacketValidatorTest, SequenceNumberEqualToExpected) {
    Header h{0u, 10u};  // sequence equal to last accepted must be rejected (no reuse)
    auto res = validatePacket(h, 10u, 5u);
    EXPECT_EQ(res, PacketValidator::Status::SequenceNumberInvalid);
}

TEST(PacketValidatorTest, SequenceNumberAtWindowBoundary) {
    Header h{0u, 15u};  // expected 10, window 5 -> distance = 5 = window -> valid
    auto res = validatePacket(h, 10u, 5u);
    EXPECT_EQ(res, PacketValidator::Status::Valid);
}

// The flight counter is persisted on a rate-group tick, so after an unplanned reboot the restored value
// lags the last accepted sequence number by up to one tick of accepted frames. Ground's next sequence
// number is accepted iff that lag is strictly inside SEQ_NUM_WINDOW; these cases pin the bound the SDD
// relies on ("Crash window").
namespace {
constexpr uint32_t kDefaultWindow = 50000u;  // SEQ_NUM_WINDOW default in TcSecurityDeframer.fpp
}  // namespace

TEST(PacketValidatorTest, StaleCounterLagBelowWindowIsAccepted) {
    const uint32_t stalePersisted = 1000u;
    const uint32_t lag = 100u;  // frames accepted in one tick before the crash (UART bound is far lower)
    Header h{0u, stalePersisted + lag + 1u};
    EXPECT_EQ(validatePacket(h, stalePersisted, kDefaultWindow), PacketValidator::Status::Valid);
}

TEST(PacketValidatorTest, StaleCounterLagAtWindowBoundary) {
    // lag + 1 == window is the last accepted ground value; one more frame of lag is rejected
    const uint32_t stalePersisted = 1000u;
    Header atEdge{0u, stalePersisted + kDefaultWindow};
    Header pastEdge{0u, stalePersisted + kDefaultWindow + 1u};
    EXPECT_EQ(validatePacket(atEdge, stalePersisted, kDefaultWindow), PacketValidator::Status::Valid);
    EXPECT_EQ(validatePacket(pastEdge, stalePersisted, kDefaultWindow), PacketValidator::Status::SequenceNumberInvalid);
}

TEST(PacketValidatorTest, StaleCounterAcrossU32Wrap) {
    const uint32_t stalePersisted = 0xFFFFFFF0u;
    Header h{0u, 0x00000010u};  // 32 frames ahead across the wrap
    EXPECT_EQ(validatePacket(h, stalePersisted, kDefaultWindow), PacketValidator::Status::Valid);
}

TEST(PacketValidatorTest, ReplayOfFrameAcceptedBeforeCrashIsRejectedOnlyIfPersisted) {
    // Frames accepted after the last successful persist and before a crash are at or below the restored
    // value only if they were persisted; a frame one ahead of the stale value re-validates. This is the
    // documented write-behind trade-off.
    const uint32_t stalePersisted = 1000u;
    Header persisted{0u, stalePersisted};
    Header unpersisted{0u, stalePersisted + 1u};
    EXPECT_EQ(validatePacket(persisted, stalePersisted, kDefaultWindow),
              PacketValidator::Status::SequenceNumberInvalid);
    EXPECT_EQ(validatePacket(unpersisted, stalePersisted, kDefaultWindow), PacketValidator::Status::Valid);
}

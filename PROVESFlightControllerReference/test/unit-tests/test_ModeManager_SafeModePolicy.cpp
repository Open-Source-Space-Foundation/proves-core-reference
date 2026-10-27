#include <gtest/gtest.h>

#include "PROVESFlightControllerReference/Components/ModeManager/SafeModePolicy.hpp"

using Components::SafeModePolicy;
using Action = SafeModePolicy::Action;
using Mode = SafeModePolicy::Mode;
using Reason = SafeModePolicy::Reason;

namespace {

const SafeModePolicy::Params PARAMS = {6.7f, 8.0f, 3};

constexpr float GOOD = 7.4f;
constexpr float LOW = 6.0f;
constexpr float RECOVERED = 8.5f;

Action normalTick(SafeModePolicy& policy, float voltage, bool valid = true) {
    return policy.tick(voltage, valid, PARAMS, Mode::NORMAL, Reason::NONE);
}

Action safeTick(SafeModePolicy& policy, float voltage, bool valid, Reason reason) {
    return policy.tick(voltage, valid, PARAMS, Mode::SAFE_MODE, reason);
}

}  // namespace

// ----------------------------------------------------------------------
// Low-voltage entry
// ----------------------------------------------------------------------

TEST(SafeModePolicyEntryTest, GoodVoltageNeverEnters) {
    SafeModePolicy policy;

    for (int i = 0; i < 100; i++) {
        EXPECT_EQ(normalTick(policy, GOOD), Action::NONE);
    }
}

TEST(SafeModePolicyEntryTest, EntersAfterDebounceTicks) {
    SafeModePolicy policy;

    EXPECT_EQ(normalTick(policy, LOW), Action::NONE);
    EXPECT_EQ(normalTick(policy, LOW), Action::NONE);
    EXPECT_EQ(normalTick(policy, LOW), Action::ENTER_LOW_BATTERY);
}

TEST(SafeModePolicyEntryTest, VoltageEqualToEntryThresholdIsGood) {
    SafeModePolicy policy;

    for (int i = 0; i < 10; i++) {
        EXPECT_EQ(normalTick(policy, PARAMS.entryVoltage), Action::NONE);
    }
}

TEST(SafeModePolicyEntryTest, GoodReadingResetsCount) {
    SafeModePolicy policy;

    EXPECT_EQ(normalTick(policy, LOW), Action::NONE);
    EXPECT_EQ(normalTick(policy, LOW), Action::NONE);
    EXPECT_EQ(normalTick(policy, GOOD), Action::NONE);
    EXPECT_EQ(normalTick(policy, LOW), Action::NONE);
    EXPECT_EQ(normalTick(policy, LOW), Action::NONE);
    EXPECT_EQ(normalTick(policy, LOW), Action::ENTER_LOW_BATTERY);
}

TEST(SafeModePolicyEntryTest, InvalidReadingCountsAsFault) {
    SafeModePolicy policy;

    // A high voltage must not mask an invalid reading
    EXPECT_EQ(normalTick(policy, GOOD, false), Action::NONE);
    EXPECT_EQ(normalTick(policy, GOOD, false), Action::NONE);
    EXPECT_EQ(normalTick(policy, GOOD, false), Action::ENTER_LOW_BATTERY);
}

TEST(SafeModePolicyEntryTest, CountRestartsAfterEntering) {
    SafeModePolicy policy;

    for (int i = 0; i < 3; i++) {
        normalTick(policy, LOW);
    }

    EXPECT_EQ(normalTick(policy, LOW), Action::NONE);
    EXPECT_EQ(normalTick(policy, LOW), Action::NONE);
    EXPECT_EQ(normalTick(policy, LOW), Action::ENTER_LOW_BATTERY);
}

TEST(SafeModePolicyEntryTest, ZeroDebounceEntersOnFirstFault) {
    SafeModePolicy policy;
    const SafeModePolicy::Params immediate = {6.7f, 8.0f, 0};

    EXPECT_EQ(policy.tick(LOW, true, immediate, Mode::NORMAL, Reason::NONE), Action::ENTER_LOW_BATTERY);
}

TEST(SafeModePolicyEntryTest, CountIsClearedWhileInSafeMode) {
    SafeModePolicy policy;

    normalTick(policy, LOW);
    normalTick(policy, LOW);
    safeTick(policy, LOW, true, Reason::GROUND_COMMAND);

    // Back in NORMAL the count starts over
    EXPECT_EQ(normalTick(policy, LOW), Action::NONE);
    EXPECT_EQ(normalTick(policy, LOW), Action::NONE);
    EXPECT_EQ(normalTick(policy, LOW), Action::ENTER_LOW_BATTERY);
}

// ----------------------------------------------------------------------
// Auto-recovery
// ----------------------------------------------------------------------

TEST(SafeModePolicyRecoveryTest, ExitsAfterDebounceTicks) {
    SafeModePolicy policy;

    EXPECT_EQ(safeTick(policy, RECOVERED, true, Reason::LOW_BATTERY), Action::NONE);
    EXPECT_EQ(safeTick(policy, RECOVERED, true, Reason::LOW_BATTERY), Action::NONE);
    EXPECT_EQ(safeTick(policy, RECOVERED, true, Reason::LOW_BATTERY), Action::EXIT_AUTO);
}

TEST(SafeModePolicyRecoveryTest, VoltageEqualToRecoveryThresholdDoesNotRecover) {
    SafeModePolicy policy;

    for (int i = 0; i < 10; i++) {
        EXPECT_EQ(safeTick(policy, PARAMS.recoveryVoltage, true, Reason::LOW_BATTERY), Action::NONE);
    }
}

TEST(SafeModePolicyRecoveryTest, DipResetsCount) {
    SafeModePolicy policy;

    safeTick(policy, RECOVERED, true, Reason::LOW_BATTERY);
    safeTick(policy, RECOVERED, true, Reason::LOW_BATTERY);
    EXPECT_EQ(safeTick(policy, GOOD, true, Reason::LOW_BATTERY), Action::NONE);
    EXPECT_EQ(safeTick(policy, RECOVERED, true, Reason::LOW_BATTERY), Action::NONE);
    EXPECT_EQ(safeTick(policy, RECOVERED, true, Reason::LOW_BATTERY), Action::NONE);
    EXPECT_EQ(safeTick(policy, RECOVERED, true, Reason::LOW_BATTERY), Action::EXIT_AUTO);
}

TEST(SafeModePolicyRecoveryTest, InvalidReadingResetsCount) {
    SafeModePolicy policy;

    safeTick(policy, RECOVERED, true, Reason::LOW_BATTERY);
    safeTick(policy, RECOVERED, true, Reason::LOW_BATTERY);
    EXPECT_EQ(safeTick(policy, RECOVERED, false, Reason::LOW_BATTERY), Action::NONE);
    EXPECT_EQ(safeTick(policy, RECOVERED, true, Reason::LOW_BATTERY), Action::NONE);
}

TEST(SafeModePolicyRecoveryTest, OnlyLowBatteryRecoversAutomatically) {
    const Reason others[] = {Reason::NONE, Reason::SYSTEM_FAULT, Reason::GROUND_COMMAND, Reason::EXTERNAL_REQUEST,
                             Reason::LORA, Reason::COMMAND_LOSS};

    for (Reason reason : others) {
        SafeModePolicy policy;
        for (int i = 0; i < 100; i++) {
            EXPECT_EQ(safeTick(policy, RECOVERED, true, reason), Action::NONE);
        }
    }
}

TEST(SafeModePolicyRecoveryTest, CountIsClearedInNormalMode) {
    SafeModePolicy policy;

    safeTick(policy, RECOVERED, true, Reason::LOW_BATTERY);
    safeTick(policy, RECOVERED, true, Reason::LOW_BATTERY);
    normalTick(policy, GOOD);

    EXPECT_EQ(safeTick(policy, RECOVERED, true, Reason::LOW_BATTERY), Action::NONE);
    EXPECT_EQ(safeTick(policy, RECOVERED, true, Reason::LOW_BATTERY), Action::NONE);
    EXPECT_EQ(safeTick(policy, RECOVERED, true, Reason::LOW_BATTERY), Action::EXIT_AUTO);
}

// ----------------------------------------------------------------------
// Command loss
// ----------------------------------------------------------------------

TEST(SafeModePolicyCommandLossTest, FiresAtTimeout) {
    SafeModePolicy policy;

    policy.commandLossTick();
    policy.commandLossTick();
    EXPECT_EQ(policy.commandLossCheck(3), Action::NONE);
    policy.commandLossTick();
    EXPECT_EQ(policy.commandLossCheck(3), Action::ENTER_COMMAND_LOSS);
    EXPECT_EQ(policy.commandLossSeconds(), 3u);
}

TEST(SafeModePolicyCommandLossTest, FiresOnlyOnceUntilPacketReceived) {
    SafeModePolicy policy;

    for (int i = 0; i < 5; i++) {
        policy.commandLossTick();
    }

    EXPECT_EQ(policy.commandLossCheck(3), Action::ENTER_COMMAND_LOSS);
    policy.commandLossTick();
    EXPECT_EQ(policy.commandLossCheck(3), Action::NONE);
    EXPECT_EQ(policy.commandLossCheck(3), Action::NONE);
}

TEST(SafeModePolicyCommandLossTest, PacketRearms) {
    SafeModePolicy policy;

    for (int i = 0; i < 3; i++) {
        policy.commandLossTick();
    }
    EXPECT_EQ(policy.commandLossCheck(3), Action::ENTER_COMMAND_LOSS);

    policy.packetReceived();
    EXPECT_EQ(policy.commandLossSeconds(), 0u);
    EXPECT_EQ(policy.commandLossCheck(3), Action::NONE);

    for (int i = 0; i < 3; i++) {
        policy.commandLossTick();
    }
    EXPECT_EQ(policy.commandLossCheck(3), Action::ENTER_COMMAND_LOSS);
}

TEST(SafeModePolicyCommandLossTest, PacketResetsTimer) {
    SafeModePolicy policy;

    policy.commandLossTick();
    policy.commandLossTick();
    policy.packetReceived();
    policy.commandLossTick();

    EXPECT_EQ(policy.commandLossCheck(3), Action::NONE);
}

TEST(SafeModePolicyCommandLossTest, ZeroTimeoutFiresImmediately) {
    SafeModePolicy policy;

    EXPECT_EQ(policy.commandLossCheck(0), Action::ENTER_COMMAND_LOSS);
}

// ----------------------------------------------------------------------
// Persisted state
// ----------------------------------------------------------------------

TEST(SafeModePolicyStateTest, RoundTrip) {
    const auto bytes = SafeModePolicy::encodeState(Mode::SAFE_MODE, 0x01020304u, Reason::COMMAND_LOSS, true);
    const auto state = SafeModePolicy::decodeState(bytes);

    EXPECT_EQ(state.status, SafeModePolicy::DecodedState::Status::OK);
    EXPECT_EQ(state.mode, Mode::SAFE_MODE);
    EXPECT_EQ(state.safeModeEntryCount, 0x01020304u);
    EXPECT_EQ(state.reason, Reason::COMMAND_LOSS);
    EXPECT_FALSE(state.unintendedReboot);
}

TEST(SafeModePolicyStateTest, EncodeMatchesLegacyStructLayout) {
    const auto bytes = SafeModePolicy::encodeState(Mode::NORMAL, 0x01020304u, Reason::LORA, true);

    // mode, 3 padding, count little-endian, reason, clean shutdown, 2 padding
    const SafeModePolicy::StateBytes expected = {2, 0, 0, 0, 0x04, 0x03, 0x02, 0x01, 5, 1, 0, 0};
    EXPECT_EQ(bytes, expected);
}

TEST(SafeModePolicyStateTest, DecodeIgnoresPaddingBytes) {
    // The old struct left its padding uninitialized, so files hold arbitrary bytes there
    const SafeModePolicy::StateBytes bytes = {1, 0xAA, 0xBB, 0xCC, 7, 0, 0, 0, 3, 1, 0xDD, 0xEE};
    const auto state = SafeModePolicy::decodeState(bytes);

    EXPECT_EQ(state.status, SafeModePolicy::DecodedState::Status::OK);
    EXPECT_EQ(state.mode, Mode::SAFE_MODE);
    EXPECT_EQ(state.safeModeEntryCount, 7u);
    EXPECT_EQ(state.reason, Reason::GROUND_COMMAND);
}

TEST(SafeModePolicyStateTest, ModeOutOfRangeIsCorrupt) {
    for (std::uint8_t mode : {0, 3, 255}) {
        SafeModePolicy::StateBytes bytes = SafeModePolicy::encodeState(Mode::SAFE_MODE, 9, Reason::LOW_BATTERY, false);
        bytes[0] = mode;
        const auto state = SafeModePolicy::decodeState(bytes);

        EXPECT_EQ(state.status, SafeModePolicy::DecodedState::Status::CORRUPT_MODE);
        EXPECT_EQ(state.rawMode, mode);
        EXPECT_EQ(state.mode, Mode::NORMAL);
        EXPECT_EQ(state.safeModeEntryCount, 0u);
        EXPECT_EQ(state.reason, Reason::NONE);
        EXPECT_FALSE(state.unintendedReboot);
    }
}

TEST(SafeModePolicyStateTest, UncleanShutdownInNormalIsUnintendedReboot) {
    const auto bytes = SafeModePolicy::encodeState(Mode::NORMAL, 0, Reason::NONE, false);

    EXPECT_TRUE(SafeModePolicy::decodeState(bytes).unintendedReboot);
}

TEST(SafeModePolicyStateTest, CleanShutdownInNormalIsIntended) {
    const auto bytes = SafeModePolicy::encodeState(Mode::NORMAL, 0, Reason::NONE, true);

    EXPECT_FALSE(SafeModePolicy::decodeState(bytes).unintendedReboot);
}

TEST(SafeModePolicyStateTest, UncleanShutdownInSafeModeIsNotUnintendedReboot) {
    const auto bytes = SafeModePolicy::encodeState(Mode::SAFE_MODE, 1, Reason::LOW_BATTERY, false);

    EXPECT_FALSE(SafeModePolicy::decodeState(bytes).unintendedReboot);
}

TEST(SafeModePolicyStateTest, AnyNonZeroCleanShutdownByteIsClean) {
    SafeModePolicy::StateBytes bytes = SafeModePolicy::encodeState(Mode::NORMAL, 0, Reason::NONE, false);
    bytes[9] = 2;

    EXPECT_FALSE(SafeModePolicy::decodeState(bytes).unintendedReboot);
}

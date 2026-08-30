// ======================================================================
// \title  ModeManagerTestMain.cpp
// \author ncc-michael
// \brief  cpp file for ModeManager component test main function
// ======================================================================

#include "ModeManagerTester.hpp"

TEST(Nominal, InitialState) {
    Components::ModeManagerTester tester;
    tester.testInitialState();
}

TEST(Nominal, ForceSafeModeCommand) {
    Components::ModeManagerTester tester;
    tester.testForceSafeModeCommand();
}

TEST(Nominal, ForceSafeModeIdempotent) {
    Components::ModeManagerTester tester;
    tester.testForceSafeModeIdempotent();
}

TEST(Nominal, ExitSafeModeCommand) {
    Components::ModeManagerTester tester;
    tester.testExitSafeModeCommand();
}

TEST(Nominal, ExitSafeModeWhenAlreadyNormal) {
    Components::ModeManagerTester tester;
    tester.testExitSafeModeWhenAlreadyNormal();
}

TEST(Nominal, ModeQueryCommandsInSafeMode) {
    Components::ModeManagerTester tester;
    tester.testModeQueryCommandsInSafeMode();
}

TEST(ForceSafeModePort, DefaultsReason) {
    Components::ModeManagerTester tester;
    tester.testForceSafeModePortDefaultsReason();
}

TEST(ForceSafeModePort, ExplicitReason) {
    Components::ModeManagerTester tester;
    tester.testForceSafeModePortExplicitReason();
}

TEST(ForceSafeModePort, IgnoredInSafeMode) {
    Components::ModeManagerTester tester;
    tester.testForceSafeModePortIgnoredInSafeMode();
}

TEST(Voltage, EntryThresholdBoundary) {
    Components::ModeManagerTester tester;
    tester.testVoltageEntryThresholdBoundary();
}

TEST(Voltage, EntryDebounceDefaultTenSeconds) {
    Components::ModeManagerTester tester;
    tester.testVoltageEntryDebounceDefaultTenSeconds();
}

TEST(Voltage, DebounceResetOnRecovery) {
    Components::ModeManagerTester tester;
    tester.testVoltageDebounceResetOnRecovery();
}

TEST(Voltage, RecoveryThresholdBoundary) {
    Components::ModeManagerTester tester;
    tester.testVoltageRecoveryThresholdBoundary();
}

TEST(Voltage, RecoveryDebounce) {
    Components::ModeManagerTester tester;
    tester.testVoltageRecoveryDebounce();
}

TEST(Voltage, NoAutoRecoveryForNonLowBatteryReasons) {
    Components::ModeManagerTester tester;
    tester.testNoAutoRecoveryForNonLowBatteryReasons();
}

TEST(Sequence, CompletionOk) {
    Components::ModeManagerTester tester;
    tester.testSequenceCompletionOk();
}

TEST(Sequence, CompletionFailure) {
    Components::ModeManagerTester tester;
    tester.testSequenceCompletionFailure();
}

TEST(CommandLoss, TriggersSafeModeAndWatchdogStop) {
    Components::ModeManagerTester tester;
    tester.testCommandLossTriggersSafeModeAndWatchdogStop();
}

TEST(CommandLoss, PacketRoutedResetsTimer) {
    Components::ModeManagerTester tester;
    tester.testPacketRoutedResetsCommandLossTimer();
}

TEST(Reboot, PrepareForReboot) {
    Components::ModeManagerTester tester;
    tester.testPrepareForReboot();
}

TEST(Persistence, RestoreUnintendedReboot) {
    Components::ModeManagerTester tester(true);
    tester.testRestoreUnintendedReboot();
}

TEST(Persistence, RestoreCleanShutdown) {
    Components::ModeManagerTester tester(true);
    tester.testRestoreCleanShutdown();
}

TEST(Persistence, RestoreNoStateFile) {
    Components::ModeManagerTester tester(true);
    tester.testRestoreNoStateFile();
}

TEST(Persistence, RestoreSafeModeState) {
    Components::ModeManagerTester tester(true);
    tester.testRestoreSafeModeState();
}

TEST(Persistence, RestoreShortStateFile) {
    Components::ModeManagerTester tester(true);
    tester.testRestoreShortStateFile();
}

TEST(Persistence, RestoreCorruptModeValue) {
    Components::ModeManagerTester tester(true);
    tester.testRestoreCorruptModeValue();
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

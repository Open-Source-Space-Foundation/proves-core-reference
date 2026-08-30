// ======================================================================
// \title  ModeManagerTester.cpp
// \author ncc-michael
// \brief  cpp file for ModeManager component test harness implementation class
// ======================================================================

#include "ModeManagerTester.hpp"

#include "Fw/Time/TimeIntervalValueSerializableAc.hpp"
#include "Os/File.hpp"
#include "Os/FileSystem.hpp"

namespace Components {

// Default value of the SAFEMODE_SEQUENCE_FILE parameter (see ModeManager.fpp)
static const char* const SAFE_MODE_SEQUENCE_FILE = "/seq/enter_safe.bin";

// ----------------------------------------------------------------------
// Construction and destruction
// ----------------------------------------------------------------------

ModeManagerTester ::ModeManagerTester(bool deferBoot)
    : ModeManagerGTestBase("ModeManagerTester", ModeManagerTester::MAX_HISTORY_SIZE), component("ModeManager") {
    this->initComponents();
    this->connectPorts();
    // Hermetic persistence: point the component's state file at a per-test
    // path under the current working directory instead of the flight default
    // /mode_state.bin (whose writability depends on the host environment),
    // and start every test from a clean slate
    this->component.m_stateFilePath = TEST_STATE_FILE;
    (void)Os::FileSystem::removeFile(TEST_STATE_FILE);
    if (!deferBoot) {
        // Parameter valid flags start UNINIT; both commandLossCheck() and
        // runSafeModeSequence() FW_ASSERT on them, so parameters must be loaded
        // (picking up the FPP defaults) before the first handler runs
        this->component.loadParameters();
    }
    // deferBoot=true: the persistence tests seed the state file first and then
    // call bootFromPersistentState(), which mirrors the topology boot ordering
}

ModeManagerTester ::~ModeManagerTester() {
    (void)Os::FileSystem::removeFile(TEST_STATE_FILE);
    this->component.deinit();
}

// ----------------------------------------------------------------------
// Tests
// ----------------------------------------------------------------------

void ModeManagerTester ::testInitialState() {
    // With no persisted state file the component boots in NORMAL mode
    ASSERT_EQ(this->invoke_to_getMode(0), Components::SystemMode::NORMAL);

    // The query commands succeed and report NORMAL / NONE via events
    this->sendCmd_GET_CURRENT_MODE(0, 1);
    this->sendCmd_GET_SAFE_MODE_REASON(0, 2);
    ASSERT_CMD_RESPONSE_SIZE(2);
    ASSERT_CMD_RESPONSE(0, ModeManagerComponentBase::OPCODE_GET_CURRENT_MODE, 1, Fw::CmdResponse(Fw::CmdResponse::OK));
    ASSERT_CMD_RESPONSE(1, ModeManagerComponentBase::OPCODE_GET_SAFE_MODE_REASON, 2,
                        Fw::CmdResponse(Fw::CmdResponse::OK));
    ASSERT_EVENTS_CurrentModeReading_SIZE(1);
    ASSERT_EVENTS_CurrentModeReading(0, Components::SystemMode::NORMAL);
    ASSERT_EVENTS_CurrentSafeModeReasonReading_SIZE(1);
    ASSERT_EVENTS_CurrentSafeModeReasonReading(0, Components::SafeModeReason::NONE);

    // A nominal 1Hz tick with healthy voltage is quiet: telemetry only
    this->clearHistory();
    this->tick(1);
    ASSERT_from_voltageGet_SIZE(1);
    ASSERT_TLM_CurrentMode_SIZE(1);
    ASSERT_TLM_CurrentMode(0, 2);
    ASSERT_TLM_CurrentSafeModeReason_SIZE(1);
    ASSERT_TLM_CurrentSafeModeReason(0, Components::SafeModeReason::NONE);
    ASSERT_TLM_SafeModeEntryCount_SIZE(1);
    ASSERT_TLM_SafeModeEntryCount(0, 0);
    ASSERT_EVENTS_SIZE(0);
    ASSERT_from_runSequence_SIZE(0);
    ASSERT_from_loadSwitchTurnOff_SIZE(0);
    ASSERT_from_loadSwitchTurnOn_SIZE(0);
    ASSERT_from_stopWatchdog_SIZE(0);
    ASSERT_from_modeChanged_SIZE(0);
}

void ModeManagerTester ::testForceSafeModeCommand() {
    const U32 cmdSeq = 10;
    this->sendCmd_FORCE_SAFE_MODE(0, cmdSeq);

    // Command succeeds and reports manual entry with reason "Ground command"
    ASSERT_CMD_RESPONSE_SIZE(1);
    ASSERT_CMD_RESPONSE(0, ModeManagerComponentBase::OPCODE_FORCE_SAFE_MODE, cmdSeq,
                        Fw::CmdResponse(Fw::CmdResponse::OK));
    ASSERT_EVENTS_SIZE(2);
    ASSERT_EVENTS_ManualSafeModeEntry_SIZE(1);
    ASSERT_EVENTS_EnteringSafeMode_SIZE(1);
    ASSERT_EVENTS_EnteringSafeMode(0, "Ground command");
    // The state save to the (now writable) file succeeds silently
    ASSERT_EVENTS_StatePersistenceFailure_SIZE(0);

    // The safe mode sequence is dispatched with the default sequence file
    ASSERT_from_runSequence_SIZE(1);
    ASSERT_from_runSequence(0, Fw::String(SAFE_MODE_SEQUENCE_FILE), Svc::SeqArgs());

    // All 8 load switches are commanded off and the mode change is broadcast
    // Note: only the _SIZE assertion exists for argument-less signal ports
    ASSERT_from_loadSwitchTurnOff_SIZE(8);
    ASSERT_from_loadSwitchTurnOn_SIZE(0);
    ASSERT_from_modeChanged_SIZE(1);
    ASSERT_from_modeChanged(0, Components::SystemMode::SAFE_MODE);

    // Telemetry reflects the transition immediately (no rate group tick needed)
    ASSERT_TLM_CurrentMode(0, 1);
    ASSERT_TLM_SafeModeEntryCount(0, 1);
    ASSERT_TLM_CurrentSafeModeReason(0, Components::SafeModeReason::GROUND_COMMAND);

    ASSERT_EQ(this->invoke_to_getMode(0), Components::SystemMode::SAFE_MODE);
}

void ModeManagerTester ::testForceSafeModeIdempotent() {
    this->sendCmd_FORCE_SAFE_MODE(0, 1);
    this->clearHistory();

    // A second FORCE_SAFE_MODE while in safe mode is an OK no-op:
    // no events, no sequence, no switch activity, no mode broadcast
    this->sendCmd_FORCE_SAFE_MODE(0, 2);
    ASSERT_CMD_RESPONSE_SIZE(1);
    ASSERT_CMD_RESPONSE(0, ModeManagerComponentBase::OPCODE_FORCE_SAFE_MODE, 2, Fw::CmdResponse(Fw::CmdResponse::OK));
    ASSERT_EVENTS_SIZE(0);
    ASSERT_from_runSequence_SIZE(0);
    ASSERT_from_loadSwitchTurnOff_SIZE(0);
    ASSERT_from_modeChanged_SIZE(0);
    ASSERT_TLM_SafeModeEntryCount_SIZE(0);

    // The entry count is still 1: the next tick telemeters the same value
    this->tick(1);
    ASSERT_TLM_SafeModeEntryCount_SIZE(1);
    ASSERT_TLM_SafeModeEntryCount(0, 1);
}

void ModeManagerTester ::testExitSafeModeCommand() {
    this->sendCmd_FORCE_SAFE_MODE(0, 1);
    this->clearHistory();

    const U32 cmdSeq = 11;
    this->sendCmd_EXIT_SAFE_MODE(0, cmdSeq);

    // Command succeeds and reports the exit
    ASSERT_CMD_RESPONSE_SIZE(1);
    ASSERT_CMD_RESPONSE(0, ModeManagerComponentBase::OPCODE_EXIT_SAFE_MODE, cmdSeq,
                        Fw::CmdResponse(Fw::CmdResponse::OK));
    ASSERT_EVENTS_SIZE(1);
    ASSERT_EVENTS_ExitingSafeMode_SIZE(1);

    // All 8 load switches are commanded back on and NORMAL is broadcast
    ASSERT_from_loadSwitchTurnOn_SIZE(8);
    ASSERT_from_loadSwitchTurnOff_SIZE(0);
    ASSERT_from_modeChanged_SIZE(1);
    ASSERT_from_modeChanged(0, Components::SystemMode::NORMAL);

    // Mode and reason are updated; the entry count channel is NOT re-written on exit
    ASSERT_TLM_CurrentMode(0, 2);
    ASSERT_TLM_CurrentSafeModeReason(0, Components::SafeModeReason::NONE);
    ASSERT_TLM_SafeModeEntryCount_SIZE(0);

    ASSERT_EQ(this->invoke_to_getMode(0), Components::SystemMode::NORMAL);

    // The reason really is cleared, not just telemetered as NONE
    this->clearHistory();
    this->sendCmd_GET_SAFE_MODE_REASON(0, 12);
    ASSERT_EVENTS_CurrentSafeModeReasonReading_SIZE(1);
    ASSERT_EVENTS_CurrentSafeModeReasonReading(0, Components::SafeModeReason::NONE);
}

void ModeManagerTester ::testExitSafeModeWhenAlreadyNormal() {
    // Defined behavior: although the FPP comment says EXIT_SAFE_MODE "only
    // succeeds if currently in safe mode", the handler is unconditional. In
    // NORMAL mode it still responds OK, emits ExitingSafeMode, re-drives the
    // load switches on, and re-broadcasts NORMAL.
    this->sendCmd_EXIT_SAFE_MODE(0, 5);
    ASSERT_CMD_RESPONSE_SIZE(1);
    ASSERT_CMD_RESPONSE(0, ModeManagerComponentBase::OPCODE_EXIT_SAFE_MODE, 5, Fw::CmdResponse(Fw::CmdResponse::OK));
    ASSERT_EVENTS_SIZE(1);
    ASSERT_EVENTS_ExitingSafeMode_SIZE(1);
    ASSERT_from_loadSwitchTurnOn_SIZE(8);
    ASSERT_from_modeChanged_SIZE(1);
    ASSERT_from_modeChanged(0, Components::SystemMode::NORMAL);
    ASSERT_EQ(this->invoke_to_getMode(0), Components::SystemMode::NORMAL);
}

void ModeManagerTester ::testModeQueryCommandsInSafeMode() {
    this->sendCmd_FORCE_SAFE_MODE(0, 1);
    this->clearHistory();

    this->sendCmd_GET_CURRENT_MODE(0, 2);
    this->sendCmd_GET_SAFE_MODE_REASON(0, 3);
    ASSERT_CMD_RESPONSE_SIZE(2);
    ASSERT_CMD_RESPONSE(0, ModeManagerComponentBase::OPCODE_GET_CURRENT_MODE, 2, Fw::CmdResponse(Fw::CmdResponse::OK));
    ASSERT_CMD_RESPONSE(1, ModeManagerComponentBase::OPCODE_GET_SAFE_MODE_REASON, 3,
                        Fw::CmdResponse(Fw::CmdResponse::OK));
    ASSERT_EVENTS_CurrentModeReading_SIZE(1);
    ASSERT_EVENTS_CurrentModeReading(0, Components::SystemMode::SAFE_MODE);
    ASSERT_EVENTS_CurrentSafeModeReasonReading_SIZE(1);
    ASSERT_EVENTS_CurrentSafeModeReasonReading(0, Components::SafeModeReason::GROUND_COMMAND);
}

void ModeManagerTester ::testForceSafeModePortDefaultsReason() {
    // forceSafeMode is an async port: nothing happens until dispatch
    this->invoke_to_forceSafeMode(0, Components::SafeModeReason::NONE);
    ASSERT_from_runSequence_SIZE(0);
    ASSERT_EQ(this->invoke_to_getMode(0), Components::SystemMode::NORMAL);

    ASSERT_EQ(this->dispatchOne(this->component), ModeManagerComponentBase::MSG_DISPATCH_OK);

    // A NONE reason is promoted to EXTERNAL_REQUEST
    ASSERT_EVENTS_SIZE(2);
    ASSERT_EVENTS_ExternalFaultDetected_SIZE(1);
    ASSERT_EVENTS_EnteringSafeMode_SIZE(1);
    ASSERT_EVENTS_EnteringSafeMode(0, "External component request");
    ASSERT_from_runSequence_SIZE(1);
    ASSERT_from_runSequence(0, Fw::String(SAFE_MODE_SEQUENCE_FILE), Svc::SeqArgs());
    ASSERT_from_loadSwitchTurnOff_SIZE(8);
    ASSERT_from_modeChanged_SIZE(1);
    ASSERT_from_modeChanged(0, Components::SystemMode::SAFE_MODE);
    ASSERT_TLM_CurrentSafeModeReason(0, Components::SafeModeReason::EXTERNAL_REQUEST);
    ASSERT_EQ(this->invoke_to_getMode(0), Components::SystemMode::SAFE_MODE);
}

void ModeManagerTester ::testForceSafeModePortExplicitReason() {
    // An explicit reason (LORA) is passed through unchanged
    this->invoke_to_forceSafeMode(0, Components::SafeModeReason::LORA);
    ASSERT_EQ(this->dispatchOne(this->component), ModeManagerComponentBase::MSG_DISPATCH_OK);

    ASSERT_EVENTS_SIZE(2);
    ASSERT_EVENTS_ExternalFaultDetected_SIZE(1);
    ASSERT_EVENTS_EnteringSafeMode_SIZE(1);
    ASSERT_EVENTS_EnteringSafeMode(0, "LoRa communication fault");
    ASSERT_TLM_CurrentSafeModeReason(0, Components::SafeModeReason::LORA);
    ASSERT_EQ(this->invoke_to_getMode(0), Components::SystemMode::SAFE_MODE);
}

void ModeManagerTester ::testForceSafeModePortIgnoredInSafeMode() {
    this->sendCmd_FORCE_SAFE_MODE(0, 1);
    this->clearHistory();

    // A port request while already in safe mode is ignored with a warning
    this->invoke_to_forceSafeMode(0, Components::SafeModeReason::LORA);
    ASSERT_EQ(this->dispatchOne(this->component), ModeManagerComponentBase::MSG_DISPATCH_OK);

    ASSERT_EVENTS_SIZE(1);
    ASSERT_EVENTS_SafeModeRequestIgnored_SIZE(1);
    ASSERT_from_runSequence_SIZE(0);
    ASSERT_from_loadSwitchTurnOff_SIZE(0);
    ASSERT_from_modeChanged_SIZE(0);

    // The original reason (GROUND_COMMAND) is preserved, not overwritten
    this->clearHistory();
    this->sendCmd_GET_SAFE_MODE_REASON(0, 2);
    ASSERT_EVENTS_CurrentSafeModeReasonReading_SIZE(1);
    ASSERT_EVENTS_CurrentSafeModeReasonReading(0, Components::SafeModeReason::GROUND_COMMAND);
}

void ModeManagerTester ::testVoltageEntryThresholdBoundary() {
    // Debounce of 1 second isolates the threshold comparison itself.
    // This is the scenario the HWIL suite permanently skips (test_safe_05):
    // it cannot manipulate the battery, but the UT injects any voltage.
    this->setDebounceSeconds(1);

    // Exactly at the 6.7 V threshold: comparison is strictly below, no entry
    this->m_voltage = 6.7;
    this->tick(1);
    ASSERT_EVENTS_AutoSafeModeEntry_SIZE(0);
    ASSERT_EQ(this->invoke_to_getMode(0), Components::SystemMode::NORMAL);

    // Just above threshold: no entry
    this->m_voltage = 6.75;
    this->tick(1);
    ASSERT_EVENTS_AutoSafeModeEntry_SIZE(0);

    // Just below threshold: automatic entry with reason LOW_BATTERY
    this->clearHistory();
    this->m_voltage = 6.6875;  // exactly representable as F32
    this->tick(1);
    ASSERT_EVENTS_SIZE(2);
    ASSERT_EVENTS_AutoSafeModeEntry_SIZE(1);
    ASSERT_EVENTS_AutoSafeModeEntry(0, Components::SafeModeReason::LOW_BATTERY, 6.6875f);
    ASSERT_EVENTS_EnteringSafeMode_SIZE(1);
    ASSERT_EVENTS_EnteringSafeMode(0, "Low battery voltage");
    ASSERT_from_runSequence_SIZE(1);
    ASSERT_from_runSequence(0, Fw::String(SAFE_MODE_SEQUENCE_FILE), Svc::SeqArgs());
    ASSERT_from_loadSwitchTurnOff_SIZE(8);
    ASSERT_from_modeChanged_SIZE(1);
    ASSERT_from_modeChanged(0, Components::SystemMode::SAFE_MODE);
    ASSERT_EQ(this->invoke_to_getMode(0), Components::SystemMode::SAFE_MODE);

    // The entering tick writes the reason twice: once inside enterSafeMode
    // and once at the end of the run handler
    ASSERT_TLM_CurrentSafeModeReason_SIZE(2);
    ASSERT_TLM_CurrentSafeModeReason(1, Components::SafeModeReason::LOW_BATTERY);
}

void ModeManagerTester ::testVoltageEntryDebounceDefaultTenSeconds() {
    // Healthy ticks beforehand must not count toward the debounce
    this->tick(3);

    // 9 consecutive low-voltage seconds: still NORMAL (default debounce is 10)
    this->m_voltage = 6.5;
    this->tick(9);
    ASSERT_EVENTS_AutoSafeModeEntry_SIZE(0);
    ASSERT_EQ(this->invoke_to_getMode(0), Components::SystemMode::NORMAL);

    // The 10th consecutive low second triggers automatic entry
    this->tick(1);
    ASSERT_EVENTS_AutoSafeModeEntry_SIZE(1);
    ASSERT_EVENTS_AutoSafeModeEntry(0, Components::SafeModeReason::LOW_BATTERY, 6.5f);
    ASSERT_EQ(this->invoke_to_getMode(0), Components::SystemMode::SAFE_MODE);
}

void ModeManagerTester ::testVoltageDebounceResetOnRecovery() {
    // 9 low seconds, then one healthy second: the debounce counter resets
    this->m_voltage = 6.5;
    this->tick(9);
    this->m_voltage = 8.5;
    this->tick(1);

    // 9 more low seconds: still NORMAL because the count restarted
    this->m_voltage = 6.5;
    this->tick(9);
    ASSERT_EVENTS_AutoSafeModeEntry_SIZE(0);
    ASSERT_EQ(this->invoke_to_getMode(0), Components::SystemMode::NORMAL);

    // The 10th consecutive low second finally triggers entry
    this->tick(1);
    ASSERT_EVENTS_AutoSafeModeEntry_SIZE(1);
    ASSERT_EQ(this->invoke_to_getMode(0), Components::SystemMode::SAFE_MODE);
}

void ModeManagerTester ::testVoltageRecoveryThresholdBoundary() {
    // The scenario the HWIL suite permanently skips (test_safe_06)
    this->setDebounceSeconds(1);

    // Enter safe mode with reason LOW_BATTERY
    this->m_voltage = 6.5;
    this->tick(1);
    ASSERT_EQ(this->invoke_to_getMode(0), Components::SystemMode::SAFE_MODE);
    this->clearHistory();

    // Exactly at the 8.0 V recovery threshold: comparison is strictly above, no exit
    this->m_voltage = 8.0;
    this->tick(1);
    ASSERT_EVENTS_AutoSafeModeExit_SIZE(0);
    ASSERT_EQ(this->invoke_to_getMode(0), Components::SystemMode::SAFE_MODE);

    // Below threshold: no exit
    this->m_voltage = 7.5;
    this->tick(1);
    ASSERT_EVENTS_AutoSafeModeExit_SIZE(0);

    // Just above threshold: automatic exit, switches on, NORMAL broadcast
    this->clearHistory();
    this->m_voltage = 8.0625;  // exactly representable as F32
    this->tick(1);
    ASSERT_EVENTS_SIZE(1);
    ASSERT_EVENTS_AutoSafeModeExit_SIZE(1);
    ASSERT_EVENTS_AutoSafeModeExit(0, 8.0625f);
    ASSERT_from_loadSwitchTurnOn_SIZE(8);
    ASSERT_from_loadSwitchTurnOff_SIZE(0);
    ASSERT_from_modeChanged_SIZE(1);
    ASSERT_from_modeChanged(0, Components::SystemMode::NORMAL);
    ASSERT_EQ(this->invoke_to_getMode(0), Components::SystemMode::NORMAL);

    // The safe mode entry count survives the recovery (counts entries, not state)
    ASSERT_TLM_SafeModeEntryCount_SIZE(1);
    ASSERT_TLM_SafeModeEntryCount(0, 1);
    ASSERT_TLM_CurrentSafeModeReason(0, Components::SafeModeReason::NONE);
}

void ModeManagerTester ::testVoltageRecoveryDebounce() {
    this->setDebounceSeconds(3);

    // Enter safe mode with reason LOW_BATTERY (3 consecutive low seconds)
    this->m_voltage = 6.5;
    this->tick(3);
    ASSERT_EQ(this->invoke_to_getMode(0), Components::SystemMode::SAFE_MODE);
    this->clearHistory();

    // 2 healthy seconds: not enough for the 3-second recovery debounce
    this->m_voltage = 8.5;
    this->tick(2);
    ASSERT_EVENTS_AutoSafeModeExit_SIZE(0);

    // A dip below the recovery threshold resets the recovery counter
    this->m_voltage = 7.5;
    this->tick(1);

    // 2 more healthy seconds: still not enough because the count restarted
    this->m_voltage = 8.5;
    this->tick(2);
    ASSERT_EVENTS_AutoSafeModeExit_SIZE(0);
    ASSERT_EQ(this->invoke_to_getMode(0), Components::SystemMode::SAFE_MODE);

    // The 3rd consecutive healthy second triggers the exit
    this->tick(1);
    ASSERT_EVENTS_AutoSafeModeExit_SIZE(1);
    ASSERT_EQ(this->invoke_to_getMode(0), Components::SystemMode::NORMAL);
}

void ModeManagerTester ::testNoAutoRecoveryForNonLowBatteryReasons() {
    this->setDebounceSeconds(1);

    // Safe mode entered by ground command must NOT auto-recover on voltage
    this->sendCmd_FORCE_SAFE_MODE(0, 1);
    this->clearHistory();

    this->m_voltage = 9.0;
    this->tick(5);
    ASSERT_EVENTS_AutoSafeModeExit_SIZE(0);
    ASSERT_from_loadSwitchTurnOn_SIZE(0);
    ASSERT_from_modeChanged_SIZE(0);
    ASSERT_EQ(this->invoke_to_getMode(0), Components::SystemMode::SAFE_MODE);
}

void ModeManagerTester ::testSequenceCompletionOk() {
    // An OK sequence completion logs success and forwards the full response
    this->invoke_to_completeSequence(0, 0x123, 7, Fw::CmdResponse(Fw::CmdResponse::OK));

    ASSERT_EVENTS_SIZE(1);
    ASSERT_EVENTS_SafeModeSequenceCompleted_SIZE(1);
    ASSERT_from_sequenceDoneNotify_SIZE(1);
    ASSERT_from_sequenceDoneNotify(0, 0x123, 7, Fw::CmdResponse(Fw::CmdResponse::OK));
}

void ModeManagerTester ::testSequenceCompletionFailure() {
    // A failed sequence completion logs the response code and still forwards it
    this->invoke_to_completeSequence(0, 0x456, 8, Fw::CmdResponse(Fw::CmdResponse::EXECUTION_ERROR));

    ASSERT_EVENTS_SIZE(1);
    ASSERT_EVENTS_SafeModeSequenceFailed_SIZE(1);
    ASSERT_EVENTS_SafeModeSequenceFailed(0, Fw::CmdResponse(Fw::CmdResponse::EXECUTION_ERROR));
    ASSERT_from_sequenceDoneNotify_SIZE(1);
    ASSERT_from_sequenceDoneNotify(0, 0x456, 8, Fw::CmdResponse(Fw::CmdResponse::EXECUTION_ERROR));
}

void ModeManagerTester ::testCommandLossTriggersSafeModeAndWatchdogStop() {
    // Shrink the 3-day timeout to 3 seconds (3 rate group ticks)
    this->paramSet_COMM_LOSS_TIME(Fw::TimeIntervalValue(3, 0), Fw::ParamValid::VALID);
    this->component.loadParameters();

    // 2 ticks without a routed packet: below the timeout, nothing happens
    this->tick(2);
    ASSERT_EVENTS_CommandLossDetected_SIZE(0);
    ASSERT_from_stopWatchdog_SIZE(0);

    // The 3rd tick reaches the timeout: safe mode plus watchdog stop
    this->tick(1);
    ASSERT_EVENTS_SIZE(2);
    ASSERT_EVENTS_CommandLossDetected_SIZE(1);
    ASSERT_EVENTS_CommandLossDetected(0, 3);
    ASSERT_EVENTS_EnteringSafeMode_SIZE(1);
    ASSERT_EVENTS_EnteringSafeMode(0, "Loss of contact with ground");
    ASSERT_from_runSequence_SIZE(1);
    ASSERT_from_loadSwitchTurnOff_SIZE(8);
    ASSERT_from_stopWatchdog_SIZE(1);
    ASSERT_EQ(this->invoke_to_getMode(0), Components::SystemMode::SAFE_MODE);

    // The debounce latch prevents re-triggering while contact is still lost
    this->tick(2);
    ASSERT_EVENTS_CommandLossDetected_SIZE(1);
    ASSERT_from_stopWatchdog_SIZE(1);

    this->clearHistory();
    this->sendCmd_GET_SAFE_MODE_REASON(0, 9);
    ASSERT_EVENTS_CurrentSafeModeReasonReading_SIZE(1);
    ASSERT_EVENTS_CurrentSafeModeReasonReading(0, Components::SafeModeReason::COMMAND_LOSS);
}

void ModeManagerTester ::testPacketRoutedResetsCommandLossTimer() {
    this->paramSet_COMM_LOSS_TIME(Fw::TimeIntervalValue(3, 0), Fw::ParamValid::VALID);
    this->component.loadParameters();

    // 2 ticks, then a routed packet: the timer restarts
    this->tick(2);
    this->invoke_to_packetRouted(0);

    // 2 more ticks: only 2 seconds since the packet, no trigger
    this->tick(2);
    ASSERT_EVENTS_CommandLossDetected_SIZE(0);

    // The 3rd second since the packet triggers command loss
    this->tick(1);
    ASSERT_EVENTS_CommandLossDetected_SIZE(1);
    ASSERT_EVENTS_CommandLossDetected(0, 3);
    ASSERT_EQ(this->invoke_to_getMode(0), Components::SystemMode::SAFE_MODE);

    // Defined behavior: packetRouted also clears the debounce latch, so a
    // second timeout re-triggers the full command loss response (including
    // another safe mode entry, incrementing the count) even though the
    // component is already in SAFE_MODE
    this->clearHistory();
    this->invoke_to_packetRouted(0);
    this->tick(3);
    ASSERT_EVENTS_CommandLossDetected_SIZE(1);
    ASSERT_EVENTS_EnteringSafeMode_SIZE(1);
    ASSERT_from_stopWatchdog_SIZE(1);
    ASSERT_TLM_SafeModeEntryCount_SIZE(4);
    ASSERT_TLM_SafeModeEntryCount(3, 2);
}

void ModeManagerTester ::testPrepareForReboot() {
    // prepareForReboot logs the event without changing mode or driving switches
    this->invoke_to_prepareForReboot(0);

    ASSERT_EVENTS_SIZE(1);
    ASSERT_EVENTS_PreparingForReboot_SIZE(1);
    ASSERT_EVENTS_StatePersistenceFailure_SIZE(0);
    ASSERT_from_modeChanged_SIZE(0);
    ASSERT_from_loadSwitchTurnOn_SIZE(0);
    ASSERT_from_loadSwitchTurnOff_SIZE(0);
    ASSERT_EQ(this->invoke_to_getMode(0), Components::SystemMode::NORMAL);

    // The persisted record carries the clean-shutdown flag and current mode
    ModeManager::PersistentState state;
    ASSERT_TRUE(this->readStateFile(state));
    ASSERT_EQ(state.cleanShutdown, 1);
    ASSERT_EQ(state.mode, static_cast<U8>(Components::SystemMode::NORMAL));
    ASSERT_EQ(state.safeModeEntryCount, 0u);

    // Simulated next boot: a clean shutdown must NOT be flagged as an
    // unintended reboot
    this->clearHistory();
    this->component.restorePersistentState();
    ASSERT_EVENTS_UnintendedRebootDetected_SIZE(0);
    ASSERT_EVENTS_StatePersistenceFailure_SIZE(0);
    ASSERT_EQ(this->invoke_to_getMode(0), Components::SystemMode::NORMAL);
}

void ModeManagerTester ::testRestoreUnintendedReboot() {
    // Seed a state file recording NORMAL mode without the clean-shutdown flag:
    // the on-disk signature of a crash/watchdog/power-loss reboot
    this->seedStateFile(static_cast<U8>(Components::SystemMode::NORMAL), 5,
                        static_cast<U8>(Components::SafeModeReason::NONE), 0);
    this->bootFromPersistentState();

    // The unintended reboot is detected and the component boots into safe
    // mode with reason SYSTEM_FAULT
    ASSERT_EVENTS_UnintendedRebootDetected_SIZE(1);
    ASSERT_EVENTS_EnteringSafeMode_SIZE(1);
    ASSERT_EVENTS_EnteringSafeMode(0, "System fault (unintended reboot)");
    ASSERT_EVENTS_StatePersistenceFailure_SIZE(0);
    ASSERT_EQ(this->invoke_to_getMode(0), Components::SystemMode::SAFE_MODE);

    // NORMAL is restored first (switches on), then the fault entry turns the
    // non-critical switches off and broadcasts the change
    ASSERT_from_loadSwitchTurnOn_SIZE(8);
    ASSERT_from_loadSwitchTurnOff_SIZE(8);
    ASSERT_from_modeChanged_SIZE(1);
    ASSERT_from_modeChanged(0, Components::SystemMode::SAFE_MODE);

    // Current behavior: the safe mode sequence is NOT run on boot-time entry
    // (runSafeModeSequence() is commented out in loadState: it crashed the
    // board when run this early)
    ASSERT_from_runSequence_SIZE(0);

    // The restored entry count (5) is incremented by the fault entry
    ASSERT_TLM_SafeModeEntryCount_SIZE(1);
    ASSERT_TLM_SafeModeEntryCount(0, 6);
    ASSERT_TLM_CurrentMode(0, 1);
    ASSERT_TLM_CurrentSafeModeReason(0, Components::SafeModeReason::SYSTEM_FAULT);

    // The re-saved record reflects safe mode and re-arms crash detection
    ModeManager::PersistentState state;
    ASSERT_TRUE(this->readStateFile(state));
    ASSERT_EQ(state.mode, static_cast<U8>(Components::SystemMode::SAFE_MODE));
    ASSERT_EQ(state.safeModeEntryCount, 6u);
    ASSERT_EQ(state.safeModeReason, static_cast<U8>(Components::SafeModeReason::SYSTEM_FAULT));
    ASSERT_EQ(state.cleanShutdown, 0);
}

void ModeManagerTester ::testRestoreCleanShutdown() {
    // A clean-shutdown record (as prepareForReboot writes) restores NORMAL
    this->seedStateFile(static_cast<U8>(Components::SystemMode::NORMAL), 3,
                        static_cast<U8>(Components::SafeModeReason::NONE), 1);
    this->bootFromPersistentState();

    // Quiet boot: no fault detection, no warnings
    ASSERT_EVENTS_SIZE(0);
    ASSERT_EQ(this->invoke_to_getMode(0), Components::SystemMode::NORMAL);
    ASSERT_from_loadSwitchTurnOn_SIZE(8);
    ASSERT_from_loadSwitchTurnOff_SIZE(0);
    ASSERT_from_modeChanged_SIZE(0);

    // The persisted entry count survives the reboot
    this->tick(1);
    ASSERT_TLM_SafeModeEntryCount_SIZE(1);
    ASSERT_TLM_SafeModeEntryCount(0, 3);

    // The clean-shutdown flag is re-armed (cleared) so a future crash is
    // detected as an unintended reboot
    ModeManager::PersistentState state;
    ASSERT_TRUE(this->readStateFile(state));
    ASSERT_EQ(state.mode, static_cast<U8>(Components::SystemMode::NORMAL));
    ASSERT_EQ(state.safeModeEntryCount, 3u);
    ASSERT_EQ(state.cleanShutdown, 0);
}

void ModeManagerTester ::testRestoreNoStateFile() {
    // First boot: no state file exists
    this->bootFromPersistentState();

    // Defaults, quietly: DOESNT_EXIST is expected and logs no warning
    ASSERT_EVENTS_SIZE(0);
    ASSERT_EQ(this->invoke_to_getMode(0), Components::SystemMode::NORMAL);
    ASSERT_from_loadSwitchTurnOn_SIZE(8);
    ASSERT_from_loadSwitchTurnOff_SIZE(0);
    ASSERT_from_modeChanged_SIZE(0);

    // A fresh unclean record is written so the next boot can detect a crash
    ModeManager::PersistentState state;
    ASSERT_TRUE(this->readStateFile(state));
    ASSERT_EQ(state.mode, static_cast<U8>(Components::SystemMode::NORMAL));
    ASSERT_EQ(state.safeModeEntryCount, 0u);
    ASSERT_EQ(state.cleanShutdown, 0);
}

void ModeManagerTester ::testRestoreSafeModeState() {
    // A persisted SAFE_MODE record (clean-shutdown flag is only evaluated for
    // NORMAL) restores safe mode without a fresh entry
    this->seedStateFile(static_cast<U8>(Components::SystemMode::SAFE_MODE), 2,
                        static_cast<U8>(Components::SafeModeReason::GROUND_COMMAND), 0);
    this->bootFromPersistentState();

    // Restore, not entry: switches off and the restore event, but no reboot
    // detection, no mode broadcast, no sequence, and no count increment
    ASSERT_EVENTS_SIZE(1);
    ASSERT_EVENTS_EnteringSafeMode_SIZE(1);
    ASSERT_EVENTS_EnteringSafeMode(0, "State restored from persistent storage");
    ASSERT_EVENTS_UnintendedRebootDetected_SIZE(0);
    ASSERT_from_loadSwitchTurnOff_SIZE(8);
    ASSERT_from_loadSwitchTurnOn_SIZE(0);
    ASSERT_from_modeChanged_SIZE(0);
    ASSERT_from_runSequence_SIZE(0);
    ASSERT_EQ(this->invoke_to_getMode(0), Components::SystemMode::SAFE_MODE);

    // Entry count and reason are preserved from the record
    this->tick(1);
    ASSERT_TLM_SafeModeEntryCount(0, 2);
    ASSERT_TLM_CurrentSafeModeReason(0, Components::SafeModeReason::GROUND_COMMAND);
}

void ModeManagerTester ::testRestoreShortStateFile() {
    // A truncated state file (successful read, too few bytes)
    const U8 shortData[3] = {2, 0, 0};
    this->seedRawStateFile(shortData, sizeof(shortData));
    this->bootFromPersistentState();

    // Defined behavior: load-read warning (read status itself is OP_OK = 0),
    // then defaults - NORMAL mode, zero count, switches on
    ASSERT_EVENTS_SIZE(1);
    ASSERT_EVENTS_StatePersistenceFailure_SIZE(1);
    ASSERT_EVENTS_StatePersistenceFailure(0, "load-read", 0);
    ASSERT_EVENTS_UnintendedRebootDetected_SIZE(0);
    ASSERT_EQ(this->invoke_to_getMode(0), Components::SystemMode::NORMAL);
    ASSERT_from_loadSwitchTurnOn_SIZE(8);
    ASSERT_from_loadSwitchTurnOff_SIZE(0);

    this->clearHistory();
    this->tick(1);
    ASSERT_TLM_SafeModeEntryCount(0, 0);
}

void ModeManagerTester ::testRestoreCorruptModeValue() {
    // A full-size record whose mode value (7) is outside SAFE_MODE..NORMAL
    this->seedStateFile(7, 9, static_cast<U8>(Components::SafeModeReason::LOW_BATTERY), 0);
    this->bootFromPersistentState();

    // Defined behavior: load-corrupt warning carrying the bad mode value,
    // then defaults - NORMAL mode, zero count, switches on; the corrupt
    // record's count and reason are discarded, not restored
    ASSERT_EVENTS_SIZE(1);
    ASSERT_EVENTS_StatePersistenceFailure_SIZE(1);
    ASSERT_EVENTS_StatePersistenceFailure(0, "load-corrupt", 7);
    ASSERT_EVENTS_UnintendedRebootDetected_SIZE(0);
    ASSERT_EQ(this->invoke_to_getMode(0), Components::SystemMode::NORMAL);
    ASSERT_from_loadSwitchTurnOn_SIZE(8);
    ASSERT_from_loadSwitchTurnOff_SIZE(0);

    this->clearHistory();
    this->tick(1);
    ASSERT_TLM_SafeModeEntryCount(0, 0);
    ASSERT_TLM_CurrentSafeModeReason(0, Components::SafeModeReason::NONE);
}

// ----------------------------------------------------------------------
// Handler overrides for typed from ports
// ----------------------------------------------------------------------

F64 ModeManagerTester ::from_voltageGet_handler(FwIndexType portNum) {
    this->pushFromPortEntry_voltageGet();
    return this->m_voltage;
}

// ----------------------------------------------------------------------
// Helper functions
// ----------------------------------------------------------------------

void ModeManagerTester ::tick(U32 count) {
    for (U32 i = 0; i < count; i++) {
        this->invoke_to_run(0, 0);
    }
}

void ModeManagerTester ::setDebounceSeconds(U32 seconds) {
    this->paramSet_SafeModeDebounceSeconds(seconds, Fw::ParamValid::VALID);
    this->component.loadParameters();
}

void ModeManagerTester ::bootFromPersistentState() {
    // Mirror ReferenceDeploymentTopology.cpp: restorePersistentState() runs
    // after the ports are connected and BEFORE loadParameters()
    this->component.restorePersistentState();
    this->component.loadParameters();
}

void ModeManagerTester ::seedStateFile(U8 mode, U32 safeModeEntryCount, U8 safeModeReason, U8 cleanShutdown) {
    ModeManager::PersistentState state;
    state.mode = mode;
    state.safeModeEntryCount = safeModeEntryCount;
    state.safeModeReason = safeModeReason;
    state.cleanShutdown = cleanShutdown;
    this->seedRawStateFile(reinterpret_cast<const U8*>(&state), sizeof(state));
}

void ModeManagerTester ::seedRawStateFile(const U8* data, FwSizeType size) {
    Os::File file;
    ASSERT_EQ(file.open(TEST_STATE_FILE, Os::File::OPEN_CREATE, Os::File::OVERWRITE), Os::File::OP_OK);
    FwSizeType bytesWritten = size;
    ASSERT_EQ(file.write(data, bytesWritten, Os::File::WaitType::WAIT), Os::File::OP_OK);
    ASSERT_EQ(bytesWritten, size);
    file.close();
}

bool ModeManagerTester ::readStateFile(ModeManager::PersistentState& state) {
    Os::File file;
    if (file.open(TEST_STATE_FILE, Os::File::OPEN_READ) != Os::File::OP_OK) {
        return false;
    }
    FwSizeType bytesRead = sizeof(state);
    Os::File::Status status = file.read(reinterpret_cast<U8*>(&state), bytesRead, Os::File::WaitType::WAIT);
    file.close();
    return (status == Os::File::OP_OK) && (bytesRead == sizeof(state));
}

}  // namespace Components

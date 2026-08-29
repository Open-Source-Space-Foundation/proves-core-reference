// ======================================================================
// \title  ModeManagerTester.cpp
// \author ncc-michael
// \brief  cpp file for ModeManager component test harness implementation class
// ======================================================================

#include "ModeManagerTester.hpp"

#include "Fw/Time/TimeIntervalValueSerializableAc.hpp"

namespace Components {

// Default value of the SAFEMODE_SEQUENCE_FILE parameter (see ModeManager.fpp)
static const char* const SAFE_MODE_SEQUENCE_FILE = "/seq/enter_safe.bin";

// ----------------------------------------------------------------------
// Construction and destruction
// ----------------------------------------------------------------------

ModeManagerTester ::ModeManagerTester()
    : ModeManagerGTestBase("ModeManagerTester", ModeManagerTester::MAX_HISTORY_SIZE), component("ModeManager") {
    this->initComponents();
    this->connectPorts();
    // Parameter valid flags start UNINIT; both commandLossCheck() and
    // runSafeModeSequence() FW_ASSERT on them, so parameters must be loaded
    // (picking up the FPP defaults) before the first handler runs
    this->component.loadParameters();
}

ModeManagerTester ::~ModeManagerTester() {
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
    ASSERT_EVENTS_ManualSafeModeEntry_SIZE(1);
    ASSERT_EVENTS_EnteringSafeMode_SIZE(1);
    ASSERT_EVENTS_EnteringSafeMode(0, "Ground command");

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
    // prepareForReboot logs the event and persists a clean shutdown flag.
    // The file write targets the absolute path /mode_state.bin, whose outcome
    // is environment-dependent on a native host, so only the component
    // behavior (event, no mode change, no side effects) is asserted.
    this->invoke_to_prepareForReboot(0);

    ASSERT_EVENTS_PreparingForReboot_SIZE(1);
    ASSERT_from_modeChanged_SIZE(0);
    ASSERT_from_loadSwitchTurnOn_SIZE(0);
    ASSERT_from_loadSwitchTurnOff_SIZE(0);
    ASSERT_EQ(this->invoke_to_getMode(0), Components::SystemMode::NORMAL);
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

}  // namespace Components

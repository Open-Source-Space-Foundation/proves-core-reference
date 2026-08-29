// ======================================================================
// \title  WatchdogTester.cpp
// \author ncc-michael
// \brief  cpp file for Watchdog component test harness implementation class
// ======================================================================

#include "WatchdogTester.hpp"

namespace Components {

// ----------------------------------------------------------------------
// Construction and destruction
// ----------------------------------------------------------------------

WatchdogTester ::WatchdogTester()
    : WatchdogGTestBase("WatchdogTester", WatchdogTester::MAX_HISTORY_SIZE), component("Watchdog") {
    this->initComponents();
    this->connectPorts();
}

WatchdogTester ::~WatchdogTester() {
    this->component.deinit();
}

// ----------------------------------------------------------------------
// Tests
// ----------------------------------------------------------------------

void WatchdogTester ::testRunTogglesGpioWhileStarted() {
    // The watchdog boots running: each rate group call must toggle the pet GPIO
    this->invoke_to_run(0, 0);
    this->invoke_to_run(0, 0);
    this->invoke_to_run(0, 0);

    // GPIO starts LOW at boot, so pets go HIGH, LOW, HIGH
    ASSERT_from_gpioSet_SIZE(3);
    ASSERT_from_gpioSet(0, Fw::Logic::HIGH);
    ASSERT_from_gpioSet(1, Fw::Logic::LOW);
    ASSERT_from_gpioSet(2, Fw::Logic::HIGH);

    // Transition count telemetry increments once per rate group call
    ASSERT_TLM_WatchdogTransitions_SIZE(3);
    ASSERT_TLM_WatchdogTransitions(0, 1);
    ASSERT_TLM_WatchdogTransitions(1, 2);
    ASSERT_TLM_WatchdogTransitions(2, 3);

    // Nominal petting is quiet: no events
    ASSERT_EVENTS_SIZE(0);
}

void WatchdogTester ::testStartCommand() {
    const U32 cmdSeq = 10;
    this->sendCmd_START_WATCHDOG(0, cmdSeq);

    // Command succeeds and reports the start
    ASSERT_CMD_RESPONSE_SIZE(1);
    ASSERT_CMD_RESPONSE(0, WatchdogComponentBase::OPCODE_START_WATCHDOG, cmdSeq, Fw::CmdResponse(Fw::CmdResponse::OK));
    ASSERT_EVENTS_SIZE(1);
    ASSERT_EVENTS_WatchdogStart_SIZE(1);

    // The current transition count is published immediately on start
    ASSERT_TLM_WatchdogTransitions_SIZE(1);
    ASSERT_TLM_WatchdogTransitions(0, 0);

    // Starting alone touches neither the GPIO nor the reboot notification
    ASSERT_from_gpioSet_SIZE(0);
    ASSERT_from_prepareForReboot_SIZE(0);

    // Petting is enabled: the next rate group call pets the GPIO
    this->invoke_to_run(0, 0);
    ASSERT_from_gpioSet_SIZE(1);
    ASSERT_from_gpioSet(0, Fw::Logic::HIGH);
}

void WatchdogTester ::testStopCommand() {
    const U32 cmdSeq = 11;
    this->sendCmd_STOP_WATCHDOG(0, cmdSeq);

    // Command succeeds and reports the stop
    ASSERT_CMD_RESPONSE_SIZE(1);
    ASSERT_CMD_RESPONSE(0, WatchdogComponentBase::OPCODE_STOP_WATCHDOG, cmdSeq, Fw::CmdResponse(Fw::CmdResponse::OK));
    ASSERT_EVENTS_SIZE(1);
    ASSERT_EVENTS_WatchdogStop_SIZE(1);

    // The commanded stop warns the ModeManager that a hardware reboot is coming
    // Note: only the _SIZE assertion exists for argument-less signal ports
    ASSERT_from_prepareForReboot_SIZE(1);

    // Petting is disabled: rate group calls no longer touch the GPIO or telemetry
    this->clearHistory();
    this->invoke_to_run(0, 0);
    this->invoke_to_run(0, 0);
    ASSERT_from_gpioSet_SIZE(0);
    ASSERT_TLM_WatchdogTransitions_SIZE(0);
    ASSERT_EVENTS_SIZE(0);
}

void WatchdogTester ::testStartStopSignalPorts() {
    // Stop via the signal port: event only, no reboot notification (unlike the command)
    this->invoke_to_stop(0);
    ASSERT_EVENTS_SIZE(1);
    ASSERT_EVENTS_WatchdogStop_SIZE(1);
    ASSERT_from_prepareForReboot_SIZE(0);

    // Stopped: rate group calls do nothing
    this->invoke_to_run(0, 0);
    ASSERT_from_gpioSet_SIZE(0);

    // Start via the signal port: event plus fresh telemetry, then petting resumes
    this->clearHistory();
    this->invoke_to_start(0);
    ASSERT_EVENTS_SIZE(1);
    ASSERT_EVENTS_WatchdogStart_SIZE(1);
    ASSERT_TLM_WatchdogTransitions_SIZE(1);
    ASSERT_TLM_WatchdogTransitions(0, 0);

    this->invoke_to_run(0, 0);
    ASSERT_from_gpioSet_SIZE(1);
    ASSERT_from_gpioSet(0, Fw::Logic::HIGH);
}

void WatchdogTester ::testTransitionCountAcrossStopStart() {
    // Accumulate three transitions, leaving the GPIO HIGH
    this->invoke_to_run(0, 0);
    this->invoke_to_run(0, 0);
    this->invoke_to_run(0, 0);
    ASSERT_TLM_WatchdogTransitions_SIZE(3);
    ASSERT_TLM_WatchdogTransitions(2, 3);

    // Stop; rate group calls while stopped must not count
    this->clearHistory();
    this->invoke_to_stop(0);
    this->invoke_to_run(0, 0);
    this->invoke_to_run(0, 0);
    ASSERT_TLM_WatchdogTransitions_SIZE(0);

    // Restart: the counter is preserved (counts since boot), not reset
    this->clearHistory();
    this->invoke_to_start(0);
    ASSERT_TLM_WatchdogTransitions_SIZE(1);
    ASSERT_TLM_WatchdogTransitions(0, 3);

    // The GPIO state is preserved too: the next pet toggles HIGH -> LOW
    this->invoke_to_run(0, 0);
    ASSERT_TLM_WatchdogTransitions_SIZE(2);
    ASSERT_TLM_WatchdogTransitions(1, 4);
    ASSERT_from_gpioSet_SIZE(1);
    ASSERT_from_gpioSet(0, Fw::Logic::LOW);
}

void WatchdogTester ::testGpioFailureIsTolerated() {
    // Historical incident: an unopened GPIO driver locked up the rate group.
    // The component ignores the GPIO status by design, so a failing driver must
    // neither crash nor stop the petting loop.
    this->m_gpioStatus = Drv::GpioStatus::NOT_OPENED;

    this->invoke_to_run(0, 0);
    this->invoke_to_run(0, 0);

    // Petting and telemetry continue despite the failure status
    ASSERT_from_gpioSet_SIZE(2);
    ASSERT_from_gpioSet(0, Fw::Logic::HIGH);
    ASSERT_from_gpioSet(1, Fw::Logic::LOW);
    ASSERT_TLM_WatchdogTransitions_SIZE(2);
    ASSERT_TLM_WatchdogTransitions(1, 2);

    // No error reporting exists for this path today: the component stays quiet
    ASSERT_EVENTS_SIZE(0);

    // Recovery: once the driver reports OK again, petting is unaffected
    this->m_gpioStatus = Drv::GpioStatus::OP_OK;
    this->invoke_to_run(0, 0);
    ASSERT_from_gpioSet_SIZE(3);
    ASSERT_from_gpioSet(2, Fw::Logic::HIGH);
    ASSERT_TLM_WatchdogTransitions_SIZE(3);
    ASSERT_TLM_WatchdogTransitions(2, 3);
}

// ----------------------------------------------------------------------
// Handler overrides for typed from ports
// ----------------------------------------------------------------------

Drv::GpioStatus WatchdogTester ::from_gpioSet_handler(FwIndexType portNum, const Fw::Logic& state) {
    this->pushFromPortEntry_gpioSet(state);
    return this->m_gpioStatus;
}

}  // namespace Components

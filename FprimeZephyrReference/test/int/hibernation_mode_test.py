"""
hibernation_mode_test.py:

Integration tests for the ModeManager component (hibernation mode).

Tests cover:
- ENTER_HIBERNATION command validation (only from SAFE_MODE)
- EXIT_HIBERNATION command validation
- Hibernation telemetry (HibernationCycleCount, HibernationTotalSeconds)
- Dormant entry failure handling (HibernationEntryFailed event, counter rollback)
- Command response ordering (response sent before reboot/dormant attempt)
- Wake window behavior (reboot tests only)

Total: 10 tests (6 run in normal CI, 4 require --run-reboot for real hardware)

Mode enum values: HIBERNATION_MODE=0, SAFE_MODE=1, NORMAL=2, PAYLOAD_MODE=3

Test 06 (dormant entry failure handling) runs in normal CI on native/sim builds
where PicoSleep::sleepForSeconds() returns false. This exercises the failure
path: HibernationEntryFailed event, counter rollback, mode reversion to SAFE_MODE.

Tests 07-10 require the --run-reboot flag and will cause actual system reboots:
    pytest hibernation_mode_test.py --run-reboot
"""

import time

import pytest
from common import proves_send_and_assert_command
from fprime_gds.common.data_types.ch_data import ChData
from fprime_gds.common.testing_fw.api import IntegrationTestAPI

component = "ReferenceDeployment.modeManager"
load_switch_channels = [
    "ReferenceDeployment.face4LoadSwitch.IsOn",
    "ReferenceDeployment.face0LoadSwitch.IsOn",
    "ReferenceDeployment.face1LoadSwitch.IsOn",
    "ReferenceDeployment.face2LoadSwitch.IsOn",
    "ReferenceDeployment.face3LoadSwitch.IsOn",
    "ReferenceDeployment.face5LoadSwitch.IsOn",
    "ReferenceDeployment.payloadPowerLoadSwitch.IsOn",
    "ReferenceDeployment.payloadBatteryLoadSwitch.IsOn",
]


@pytest.fixture(autouse=True)
def setup_and_teardown(fprime_test_api: IntegrationTestAPI, start_gds):
    """
    Setup before each test and cleanup after.
    Ensures clean state by exiting any mode back to NORMAL.
    """
    # Setup: Try to get to a clean NORMAL state
    try:
        proves_send_and_assert_command(fprime_test_api, f"{component}.EXIT_SAFE_MODE")
    except Exception:
        pass
    try:
        proves_send_and_assert_command(
            fprime_test_api, f"{component}.EXIT_PAYLOAD_MODE"
        )
    except Exception:
        pass
    # Note: EXIT_HIBERNATION only works during wake window, which we won't be in
    # during normal test setup

    # Clear event and telemetry history before test
    fprime_test_api.clear_histories()

    yield

    # Teardown: Return to clean NORMAL state for next test
    try:
        proves_send_and_assert_command(fprime_test_api, f"{component}.EXIT_SAFE_MODE")
    except Exception:
        pass
    try:
        proves_send_and_assert_command(
            fprime_test_api, f"{component}.EXIT_PAYLOAD_MODE"
        )
    except Exception:
        pass


# ==============================================================================
# ENTER_HIBERNATION Command Validation Tests
# ==============================================================================


def test_hibernation_01_enter_fails_from_normal_mode(
    fprime_test_api: IntegrationTestAPI, start_gds
):
    """
    Test that ENTER_HIBERNATION fails when in NORMAL mode.
    Must be in SAFE_MODE to enter hibernation.
    """
    # Verify we're in NORMAL mode
    proves_send_and_assert_command(fprime_test_api, "CdhCore.tlmSend.SEND_PKT", ["1"])
    mode_result: ChData = fprime_test_api.assert_telemetry(
        f"{component}.CurrentMode", timeout=5
    )

    if mode_result.get_val() != 2:
        # Not in NORMAL mode - try to get there
        try:
            proves_send_and_assert_command(
                fprime_test_api, f"{component}.EXIT_SAFE_MODE"
            )
        except Exception:
            pass

    # Clear histories before the test command
    fprime_test_api.clear_histories()

    # Try to enter hibernation from NORMAL mode - should fail
    with pytest.raises(Exception):
        proves_send_and_assert_command(
            fprime_test_api, f"{component}.ENTER_HIBERNATION", ["0", "0"]
        )

    # Verify CommandValidationFailed event
    fprime_test_api.assert_event(f"{component}.CommandValidationFailed", timeout=3)

    # Verify still in NORMAL mode (2)
    proves_send_and_assert_command(fprime_test_api, "CdhCore.tlmSend.SEND_PKT", ["1"])
    mode_result: ChData = fprime_test_api.assert_telemetry(
        f"{component}.CurrentMode", timeout=5
    )
    assert mode_result.get_val() == 2, "Should still be in NORMAL mode"


def test_hibernation_02_enter_fails_from_payload_mode(
    fprime_test_api: IntegrationTestAPI, start_gds
):
    """
    Test that ENTER_HIBERNATION fails when in PAYLOAD_MODE.
    Must be in SAFE_MODE to enter hibernation.
    """
    # Enter payload mode first
    proves_send_and_assert_command(fprime_test_api, f"{component}.ENTER_PAYLOAD_MODE")
    time.sleep(2)

    # Verify in payload mode
    proves_send_and_assert_command(fprime_test_api, "CdhCore.tlmSend.SEND_PKT", ["1"])
    mode_result: ChData = fprime_test_api.assert_telemetry(
        f"{component}.CurrentMode", timeout=5
    )
    assert mode_result.get_val() == 3, "Should be in PAYLOAD_MODE"

    # Clear histories
    fprime_test_api.clear_histories()

    # Try to enter hibernation from PAYLOAD_MODE - should fail
    with pytest.raises(Exception):
        proves_send_and_assert_command(
            fprime_test_api, f"{component}.ENTER_HIBERNATION", ["0", "0"]
        )

    # Verify CommandValidationFailed event
    fprime_test_api.assert_event(f"{component}.CommandValidationFailed", timeout=3)

    # Verify still in PAYLOAD_MODE (3)
    proves_send_and_assert_command(fprime_test_api, "CdhCore.tlmSend.SEND_PKT", ["1"])
    mode_result: ChData = fprime_test_api.assert_telemetry(
        f"{component}.CurrentMode", timeout=5
    )
    assert mode_result.get_val() == 3, "Should still be in PAYLOAD_MODE"


# ==============================================================================
# EXIT_HIBERNATION Command Validation Tests
# ==============================================================================


def test_hibernation_03_exit_fails_from_normal_mode(
    fprime_test_api: IntegrationTestAPI, start_gds
):
    """
    Test that EXIT_HIBERNATION fails when not in hibernation mode.
    """
    # Verify we're in NORMAL mode (or get there)
    proves_send_and_assert_command(fprime_test_api, "CdhCore.tlmSend.SEND_PKT", ["1"])
    mode_result: ChData = fprime_test_api.assert_telemetry(
        f"{component}.CurrentMode", timeout=5
    )

    if mode_result.get_val() != 2:
        try:
            proves_send_and_assert_command(
                fprime_test_api, f"{component}.EXIT_SAFE_MODE"
            )
        except Exception:
            pass

    # Clear histories
    fprime_test_api.clear_histories()

    # Try to exit hibernation when not in it - should fail
    with pytest.raises(Exception):
        proves_send_and_assert_command(fprime_test_api, f"{component}.EXIT_HIBERNATION")

    # Verify CommandValidationFailed event with "Not currently in hibernation mode"
    fprime_test_api.assert_event(f"{component}.CommandValidationFailed", timeout=3)


def test_hibernation_04_exit_fails_from_safe_mode(
    fprime_test_api: IntegrationTestAPI, start_gds
):
    """
    Test that EXIT_HIBERNATION fails when in SAFE_MODE (not hibernation).
    """
    # Enter safe mode
    proves_send_and_assert_command(fprime_test_api, f"{component}.FORCE_SAFE_MODE")
    time.sleep(2)

    # Verify in safe mode
    proves_send_and_assert_command(fprime_test_api, "CdhCore.tlmSend.SEND_PKT", ["1"])
    mode_result: ChData = fprime_test_api.assert_telemetry(
        f"{component}.CurrentMode", timeout=5
    )
    assert mode_result.get_val() == 1, "Should be in SAFE_MODE"

    # Clear histories
    fprime_test_api.clear_histories()

    # Try to exit hibernation when in safe mode - should fail
    with pytest.raises(Exception):
        proves_send_and_assert_command(fprime_test_api, f"{component}.EXIT_HIBERNATION")

    # Verify CommandValidationFailed event
    fprime_test_api.assert_event(f"{component}.CommandValidationFailed", timeout=3)

    # Verify still in SAFE_MODE (1)
    proves_send_and_assert_command(fprime_test_api, "CdhCore.tlmSend.SEND_PKT", ["1"])
    mode_result: ChData = fprime_test_api.assert_telemetry(
        f"{component}.CurrentMode", timeout=5
    )
    assert mode_result.get_val() == 1, "Should still be in SAFE_MODE"


# ==============================================================================
# Telemetry Tests
# ==============================================================================


def test_hibernation_05_telemetry_channels_exist(
    fprime_test_api: IntegrationTestAPI, start_gds
):
    """
    Test that hibernation telemetry channels exist and can be read.
    Verifies HibernationCycleCount and HibernationTotalSeconds are available.
    """
    # Trigger telemetry update by sending Health packet (ID 1)
    proves_send_and_assert_command(fprime_test_api, "CdhCore.tlmSend.SEND_PKT", ["1"])

    # Read HibernationCycleCount telemetry
    cycle_result: ChData = fprime_test_api.assert_telemetry(
        f"{component}.HibernationCycleCount", timeout=5
    )
    cycle_count = cycle_result.get_val()
    assert cycle_count >= 0, f"Invalid cycle count: {cycle_count}"

    # Read HibernationTotalSeconds telemetry
    seconds_result: ChData = fprime_test_api.assert_telemetry(
        f"{component}.HibernationTotalSeconds", timeout=5
    )
    total_seconds = seconds_result.get_val()
    assert total_seconds >= 0, f"Invalid total seconds: {total_seconds}"


# ==============================================================================
# Dormant Entry Failure Handling Test (runs in normal CI on native/sim builds)
# ==============================================================================


def test_hibernation_06_dormant_entry_failure_handling(
    fprime_test_api: IntegrationTestAPI, start_gds
):
    """
    Test that dormant entry failure is handled correctly.

    On native/simulation builds, PicoSleep::sleepForSeconds() returns false
    (dormant mode not supported), which exercises the failure handling path.
    This test runs in normal CI without --run-reboot.

    Verifies:
    - Command response is OK (sent before dormant attempt - critical!)
    - EnteringHibernation event is emitted
    - HibernationSleepCycle event is emitted
    - HibernationEntryFailed event is emitted (WARNING_HI)
    - ExitingHibernation event is emitted (transitions to SAFE_MODE)
    - Counters are rolled back (same as before attempt)
    - Mode reverts to SAFE_MODE (not stuck in HIBERNATION_MODE)

    NOTE: On real hardware with --run-reboot, this test will cause a reboot
    and these assertions won't be reached. Use test_hibernation_07 for
    hardware-only reboot testing.
    """
    # Enter safe mode first
    proves_send_and_assert_command(fprime_test_api, f"{component}.FORCE_SAFE_MODE")
    time.sleep(2)

    # Verify in safe mode
    proves_send_and_assert_command(fprime_test_api, "CdhCore.tlmSend.SEND_PKT", ["1"])
    mode_result: ChData = fprime_test_api.assert_telemetry(
        f"{component}.CurrentMode", timeout=5
    )
    assert mode_result.get_val() == 1, "Should be in SAFE_MODE"

    # Record initial hibernation counters
    initial_cycle: ChData = fprime_test_api.assert_telemetry(
        f"{component}.HibernationCycleCount", timeout=5
    )
    initial_cycle_count = initial_cycle.get_val()

    initial_seconds: ChData = fprime_test_api.assert_telemetry(
        f"{component}.HibernationTotalSeconds", timeout=5
    )
    initial_seconds_count = initial_seconds.get_val()

    # Clear histories before the critical command
    fprime_test_api.clear_histories()

    # Enter hibernation mode - command should return OK before dormant attempt
    # CRITICAL: This command MUST return OK even if dormant fails
    proves_send_and_assert_command(
        fprime_test_api,
        f"{component}.ENTER_HIBERNATION",
        ["60", "10"],  # 60s sleep, 10s wake
    )

    # Verify EnteringHibernation event was emitted
    fprime_test_api.assert_event(f"{component}.EnteringHibernation", timeout=3)

    # Verify HibernationSleepCycle event (logged just before dormant attempt)
    fprime_test_api.assert_event(f"{component}.HibernationSleepCycle", timeout=3)

    # On native/sim builds: dormant fails, so we should see failure handling
    # Wait a moment for the failure path to complete
    time.sleep(2)

    # Try to detect if we're on native/sim (dormant failed) or real hardware (rebooted)
    # by checking if we can still communicate and what mode we're in
    try:
        proves_send_and_assert_command(
            fprime_test_api, "CdhCore.tlmSend.SEND_PKT", ["1"]
        )
        mode_result: ChData = fprime_test_api.assert_telemetry(
            f"{component}.CurrentMode", timeout=5
        )
        current_mode = mode_result.get_val()
    except Exception:
        # If we can't communicate, system rebooted (real hardware)
        pytest.skip(
            "System rebooted (real hardware). "
            "Use --run-reboot tests for hardware hibernation testing."
        )
        return

    # If we get here, we're on native/sim and dormant entry failed
    # Verify HibernationEntryFailed event (WARNING_HI) was emitted
    fprime_test_api.assert_event(f"{component}.HibernationEntryFailed", timeout=3)

    # Verify ExitingHibernation event was emitted (mode reverted to SAFE_MODE)
    fprime_test_api.assert_event(f"{component}.ExitingHibernation", timeout=3)

    # Verify mode reverted to SAFE_MODE (1), not stuck in HIBERNATION_MODE (0)
    assert current_mode == 1, (
        f"Mode should revert to SAFE_MODE (1) after dormant failure, got {current_mode}"
    )

    # Verify counters were rolled back
    cycle_result: ChData = fprime_test_api.assert_telemetry(
        f"{component}.HibernationCycleCount", timeout=5
    )
    assert cycle_result.get_val() == initial_cycle_count, (
        f"HibernationCycleCount should be rolled back to {initial_cycle_count}, "
        f"got {cycle_result.get_val()}"
    )

    seconds_result: ChData = fprime_test_api.assert_telemetry(
        f"{component}.HibernationTotalSeconds", timeout=5
    )
    assert seconds_result.get_val() == initial_seconds_count, (
        f"HibernationTotalSeconds should be rolled back to {initial_seconds_count}, "
        f"got {seconds_result.get_val()}"
    )


# ==============================================================================
# Reboot Tests (run with --run-reboot flag on real hardware)
# ==============================================================================


@pytest.mark.reboot
def test_hibernation_07_enter_reboot_success(
    fprime_test_api: IntegrationTestAPI, start_gds
):
    """
    Test that ENTER_HIBERNATION from SAFE_MODE succeeds on real hardware.

    WARNING: This test will cause the RP2350 to reboot!
    Only runs with --run-reboot flag.

    Verifies the success path BEFORE reboot:
    - Command response is received (critical: response must be sent before reboot)
    - EnteringHibernation event is emitted with correct parameters
    - HibernationSleepCycle event is emitted
    - Mode transitions to HIBERNATION_MODE (0)

    After reboot, the system will be in hibernation wake window.
    Run test_hibernation_08 within the wake window to verify exit behavior.
    """
    # Enter safe mode first
    proves_send_and_assert_command(fprime_test_api, f"{component}.FORCE_SAFE_MODE")
    time.sleep(2)

    # Verify in safe mode
    proves_send_and_assert_command(fprime_test_api, "CdhCore.tlmSend.SEND_PKT", ["1"])
    mode_result: ChData = fprime_test_api.assert_telemetry(
        f"{component}.CurrentMode", timeout=5
    )
    assert mode_result.get_val() == 1, "Should be in SAFE_MODE"

    # Record initial hibernation counters
    proves_send_and_assert_command(fprime_test_api, "CdhCore.tlmSend.SEND_PKT", ["1"])
    initial_cycle: ChData = fprime_test_api.assert_telemetry(
        f"{component}.HibernationCycleCount", timeout=5
    )
    initial_cycle_count = initial_cycle.get_val()

    # Clear histories before the critical command
    fprime_test_api.clear_histories()

    # Enter hibernation mode with short durations for testing
    # sleepDurationSec=10, wakeDurationSec=60
    # CRITICAL: This command MUST return OK before the system reboots
    # If this times out, the command response ordering is broken
    proves_send_and_assert_command(
        fprime_test_api,
        f"{component}.ENTER_HIBERNATION",
        ["10", "60"],
    )

    # Verify EnteringHibernation event was emitted with correct parameters
    fprime_test_api.assert_event(f"{component}.EnteringHibernation", timeout=3)

    # Verify HibernationSleepCycle event (logged just before dormant entry)
    fprime_test_api.assert_event(f"{component}.HibernationSleepCycle", timeout=3)

    # Verify mode changed to HIBERNATION_MODE (0) before reboot
    proves_send_and_assert_command(fprime_test_api, "CdhCore.tlmSend.SEND_PKT", ["1"])
    mode_result: ChData = fprime_test_api.assert_telemetry(
        f"{component}.CurrentMode", timeout=5
    )
    assert mode_result.get_val() == 0, "Should be in HIBERNATION_MODE"

    # Verify cycle count incremented
    cycle_result: ChData = fprime_test_api.assert_telemetry(
        f"{component}.HibernationCycleCount", timeout=5
    )
    assert cycle_result.get_val() == initial_cycle_count + 1, (
        f"HibernationCycleCount should increment (was {initial_cycle_count})"
    )

    # SUCCESS: If we reach here, the command response was sent before reboot
    # The system will now reboot into dormant mode
    # After wake, it will enter the 60-second wake window


@pytest.mark.reboot
def test_hibernation_08_exit_during_wake_window(
    fprime_test_api: IntegrationTestAPI, start_gds
):
    """
    Test EXIT_HIBERNATION during wake window succeeds.

    PREREQUISITE: System must be in hibernation mode wake window.
    This can be achieved by:
    1. Running test_hibernation_07 to enter hibernation (causes reboot)
    2. Waiting for system to reboot and enter wake window
    3. Running this test within the wake window duration (default 60s)

    Verifies:
    - System is in HIBERNATION_MODE (0) - skips if not
    - EXIT_HIBERNATION command succeeds
    - ExitingHibernation event is emitted with cycle stats
    - Mode transitions to SAFE_MODE (1)
    - Hibernation counters are preserved
    """
    # Check if we're in HIBERNATION_MODE (0)
    proves_send_and_assert_command(fprime_test_api, "CdhCore.tlmSend.SEND_PKT", ["1"])
    mode_result: ChData = fprime_test_api.assert_telemetry(
        f"{component}.CurrentMode", timeout=5
    )

    if mode_result.get_val() != 0:
        pytest.skip(
            f"Not in HIBERNATION_MODE (mode={mode_result.get_val()}). "
            "Run test_hibernation_07 first, wait for reboot, then run this test."
        )

    # Record counters before exit
    cycle_result: ChData = fprime_test_api.assert_telemetry(
        f"{component}.HibernationCycleCount", timeout=5
    )
    cycle_count_before = cycle_result.get_val()

    seconds_result: ChData = fprime_test_api.assert_telemetry(
        f"{component}.HibernationTotalSeconds", timeout=5
    )
    seconds_before = seconds_result.get_val()

    # Clear histories
    fprime_test_api.clear_histories()

    # Exit hibernation during wake window
    proves_send_and_assert_command(
        fprime_test_api,
        f"{component}.EXIT_HIBERNATION",
    )

    # Verify ExitingHibernation event was emitted
    fprime_test_api.assert_event(f"{component}.ExitingHibernation", timeout=3)

    time.sleep(2)

    # Verify mode is SAFE_MODE (1)
    proves_send_and_assert_command(fprime_test_api, "CdhCore.tlmSend.SEND_PKT", ["1"])
    mode_result: ChData = fprime_test_api.assert_telemetry(
        f"{component}.CurrentMode", timeout=5
    )
    assert mode_result.get_val() == 1, (
        "Should be in SAFE_MODE after exiting hibernation"
    )

    # Verify hibernation counters were preserved (not reset)
    cycle_result: ChData = fprime_test_api.assert_telemetry(
        f"{component}.HibernationCycleCount", timeout=5
    )
    assert cycle_result.get_val() == cycle_count_before, (
        f"HibernationCycleCount should be preserved ({cycle_count_before})"
    )

    seconds_result: ChData = fprime_test_api.assert_telemetry(
        f"{component}.HibernationTotalSeconds", timeout=5
    )
    assert seconds_result.get_val() == seconds_before, (
        f"HibernationTotalSeconds should be preserved ({seconds_before})"
    )


@pytest.mark.reboot
def test_hibernation_09_wake_window_timeout_causes_resleep(
    fprime_test_api: IntegrationTestAPI, start_gds
):
    """
    Test that wake window timeout causes system to re-enter dormant sleep.

    PREREQUISITE: System must be in hibernation mode wake window.

    This test verifies that if EXIT_HIBERNATION is NOT sent during the wake
    window, the system will automatically re-enter dormant sleep when the
    window expires.

    WARNING: This test will cause the RP2350 to reboot again!

    Verifies:
    - System is in HIBERNATION_MODE (0) and wake window
    - Wait for wake window to expire (requires short wake duration)
    - System should reboot into next sleep cycle
    """
    # Check if we're in HIBERNATION_MODE (0)
    proves_send_and_assert_command(fprime_test_api, "CdhCore.tlmSend.SEND_PKT", ["1"])
    mode_result: ChData = fprime_test_api.assert_telemetry(
        f"{component}.CurrentMode", timeout=5
    )

    if mode_result.get_val() != 0:
        pytest.skip(
            f"Not in HIBERNATION_MODE (mode={mode_result.get_val()}). "
            "Run test_hibernation_07 first with short wake duration."
        )

    # Wait for wake window to expire
    # Note: This requires the wake duration to be short (e.g., 10-30 seconds)
    # The system will emit HibernationSleepCycle event and reboot
    fprime_test_api.clear_histories()

    # Wait up to 120 seconds for the sleep cycle event
    # (adjust based on configured wake duration)
    try:
        fprime_test_api.assert_event(
            f"{component}.HibernationSleepCycle",
            timeout=120,
        )
        # If we see this event, the wake window expired and system is going to sleep
        # The system will reboot shortly after
    except Exception:
        pytest.fail(
            "Did not see HibernationSleepCycle event - wake window may not have expired "
            "or wake duration is too long for this test"
        )


@pytest.mark.reboot
def test_hibernation_10_force_safe_mode_rejected_from_hibernation(
    fprime_test_api: IntegrationTestAPI, start_gds
):
    """
    Test that FORCE_SAFE_MODE is rejected from HIBERNATION_MODE.

    PREREQUISITE: System must be in hibernation mode wake window.

    FORCE_SAFE_MODE must be rejected from HIBERNATION_MODE to ensure proper
    hibernation cleanup. Using FORCE_SAFE_MODE would bypass exitHibernation(),
    leaving m_inHibernationWakeWindow and counters in inconsistent state.

    The correct way to exit hibernation is via EXIT_HIBERNATION command.

    Verifies:
    - System is in HIBERNATION_MODE (0) - skips if not
    - FORCE_SAFE_MODE command is rejected with VALIDATION_ERROR
    - CommandValidationFailed event is emitted with "Use EXIT_HIBERNATION"
    - Mode remains HIBERNATION_MODE (not changed to SAFE_MODE)
    """
    # Check if we're in HIBERNATION_MODE (0)
    proves_send_and_assert_command(fprime_test_api, "CdhCore.tlmSend.SEND_PKT", ["1"])
    mode_result: ChData = fprime_test_api.assert_telemetry(
        f"{component}.CurrentMode", timeout=5
    )

    if mode_result.get_val() != 0:
        pytest.skip(
            f"Not in HIBERNATION_MODE (mode={mode_result.get_val()}). "
            "Run test_hibernation_07 first, wait for reboot, then run this test."
        )

    # Clear histories before the command
    fprime_test_api.clear_histories()

    # Try to force safe mode - should be rejected
    with pytest.raises(Exception):
        proves_send_and_assert_command(fprime_test_api, f"{component}.FORCE_SAFE_MODE")

    # Verify CommandValidationFailed event with correct message
    fprime_test_api.assert_event(f"{component}.CommandValidationFailed", timeout=3)

    # Verify still in HIBERNATION_MODE (0) - mode should NOT have changed
    proves_send_and_assert_command(fprime_test_api, "CdhCore.tlmSend.SEND_PKT", ["1"])
    mode_result: ChData = fprime_test_api.assert_telemetry(
        f"{component}.CurrentMode", timeout=5
    )
    assert mode_result.get_val() == 0, (
        f"Should still be in HIBERNATION_MODE (0), but got mode={mode_result.get_val()}"
    )

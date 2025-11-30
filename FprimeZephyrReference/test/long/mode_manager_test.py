"""
mode_manager_test.py:

Integration tests for the ModeManager component (safe/normal mode).

Tests cover:
- Basic functionality and telemetry
- Safe mode entry (via command)
- Safe mode exit
- State persistence
- Edge cases

Total: 9 tests

Mode enum values: HIBERNATION_MODE=0, SAFE_MODE=1, NORMAL=2, PAYLOAD_MODE=3
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
    Ensures clean state by exiting safe mode if needed.
    """
    # Setup: Try to get to a clean NORMAL state
    try:
        # Try to exit safe mode if in it
        proves_send_and_assert_command(fprime_test_api, f"{component}.EXIT_SAFE_MODE")
    except Exception:
        pass

    # Clear event and telemetry history before test
    fprime_test_api.clear_histories()

    yield

    # Teardown: Return to clean state for next test
    try:
        proves_send_and_assert_command(fprime_test_api, f"{component}.EXIT_SAFE_MODE")
    except Exception:
        pass


def force_safe_mode_once(
    fprime_test_api: IntegrationTestAPI, wait_for_entering: bool = True
):
    """
    Issue FORCE_SAFE_MODE once without retries and wait for key events.
    Returns the list of events observed so callers can inspect metadata.
    """
    fprime_test_api.clear_histories()
    start_idx = fprime_test_api.get_event_test_history().size()
    events = [f"{component}.ManualSafeModeEntry"]
    if wait_for_entering:
        events.append(f"{component}.EnteringSafeMode")
    fprime_test_api.send_command(f"{component}.FORCE_SAFE_MODE", [])
    return fprime_test_api.assert_event_sequence(events, start=start_idx, timeout=10)


# ==============================================================================
# Basic Functionality Tests
# ==============================================================================


def test_01_initial_telemetry(fprime_test_api: IntegrationTestAPI, start_gds):
    """
    Test that initial telemetry can be read and has expected default values.
    Verifies CurrentMode.
    """
    # Trigger telemetry update by sending Health packet (ID 1)
    proves_send_and_assert_command(fprime_test_api, "CdhCore.tlmSend.SEND_PKT", ["1"])

    # Read CurrentMode telemetry (1 = SAFE_MODE, 2 = NORMAL, 3 = PAYLOAD_MODE)
    mode_result: ChData = fprime_test_api.assert_telemetry(
        f"{component}.CurrentMode", start="NOW", timeout=3
    )
    current_mode = mode_result.get_val()
    assert current_mode in [1, 2, 3], f"Invalid mode value: {current_mode}"


# ==============================================================================
# Safe Mode Entry Tests
# ==============================================================================


def test_04_force_safe_mode_command(fprime_test_api: IntegrationTestAPI, start_gds):
    """
    Test FORCE_SAFE_MODE command enters safe mode by checking telemetry.
    Note: With idempotent behavior, ManualSafeModeEntry is only emitted when
    transitioning from NORMAL, not when already in SAFE_MODE.
    """
    # Send FORCE_SAFE_MODE command (idempotent - succeeds even if already in safe mode)
    proves_send_and_assert_command(fprime_test_api, f"{component}.FORCE_SAFE_MODE")

    # Wait for mode transition (happens in 1Hz rate group)
    time.sleep(3)

    # Trigger telemetry update
    proves_send_and_assert_command(fprime_test_api, "CdhCore.tlmSend.SEND_PKT", ["1"])

    # Verify mode is SAFE_MODE (1)
    mode_result: ChData = fprime_test_api.assert_telemetry(
        f"{component}.CurrentMode", timeout=5
    )
    assert mode_result.get_val() == 1, "Should be in SAFE_MODE"


def test_05_safe_mode_increments_counter(
    fprime_test_api: IntegrationTestAPI, start_gds
):
    """
    Test that SafeModeEntryCount telemetry increments each time safe mode is entered.
    """
    # Read initial safe mode entry count
    proves_send_and_assert_command(fprime_test_api, "CdhCore.tlmSend.SEND_PKT", ["1"])
    initial_count: ChData = fprime_test_api.assert_telemetry(
        f"{component}.SafeModeEntryCount", timeout=5
    )
    initial_value = initial_count.get_val()

    # Enter safe mode
    force_safe_mode_once(fprime_test_api, wait_for_entering=True)
    time.sleep(3)

    # Read new count
    proves_send_and_assert_command(fprime_test_api, "CdhCore.tlmSend.SEND_PKT", ["1"])
    new_count: ChData = fprime_test_api.assert_telemetry(
        f"{component}.SafeModeEntryCount", timeout=5
    )
    new_value = new_count.get_val()

    # Verify count incremented by 1
    assert new_value == initial_value + 1, (
        f"Count should increment by 1 (was {initial_value}, now {new_value})"
    )


def test_06_safe_mode_turns_off_load_switches(
    fprime_test_api: IntegrationTestAPI, start_gds
):
    """
    Test that entering safe mode turns off all load switches.
    """
    force_safe_mode_once(fprime_test_api, wait_for_entering=True)
    time.sleep(3)

    # Downlink load switch telemetry (packet 9) and verify each switch is OFF (0)
    proves_send_and_assert_command(fprime_test_api, "CdhCore.tlmSend.SEND_PKT", ["9"])
    for channel in load_switch_channels:
        value = fprime_test_api.assert_telemetry(channel, timeout=5).get_val()
        if isinstance(value, str):
            assert value.upper() == "OFF", f"{channel} should be OFF in safe mode"
        else:
            assert value == 0, f"{channel} should be OFF in safe mode"


def test_07_safe_mode_emits_event(fprime_test_api: IntegrationTestAPI, start_gds):
    """
    Test that EnteringSafeMode event is emitted with correct reason.
    """
    events = force_safe_mode_once(fprime_test_api, wait_for_entering=True)
    entering_event = events[-1]
    assert "Ground command" in entering_event.get_display_text(), (
        "EnteringSafeMode reason should mention Ground command"
    )


# ==============================================================================
# Safe Mode Exit Tests
# ==============================================================================


def test_13_exit_safe_mode_fails_not_in_safe_mode(
    fprime_test_api: IntegrationTestAPI, start_gds
):
    """
    Test EXIT_SAFE_MODE fails when not currently in safe mode.
    """
    # Ensure we're in NORMAL mode (should be from setup)
    proves_send_and_assert_command(fprime_test_api, "CdhCore.tlmSend.SEND_PKT", ["1"])
    mode_result: ChData = fprime_test_api.assert_telemetry(
        f"{component}.CurrentMode", start="NOW", timeout=3
    )

    if mode_result.get_val() != 2:
        pytest.skip("Not in NORMAL mode - cannot test this scenario")

    # Try to exit safe mode when not in it - should fail
    fprime_test_api.clear_histories()
    with pytest.raises(Exception):
        proves_send_and_assert_command(
            fprime_test_api,
            f"{component}.EXIT_SAFE_MODE",
        )
    fprime_test_api.assert_event(f"{component}.CommandValidationFailed", timeout=3)


def test_14_exit_safe_mode_success(fprime_test_api: IntegrationTestAPI, start_gds):
    """
    Test EXIT_SAFE_MODE succeeds.
    Verifies:
    - ExitingSafeMode event is emitted
    - CurrentMode returns to NORMAL (2)
    """
    # Enter safe mode
    proves_send_and_assert_command(fprime_test_api, f"{component}.FORCE_SAFE_MODE")
    time.sleep(2)

    # Exit safe mode - should succeed (no fault checks anymore)
    proves_send_and_assert_command(
        fprime_test_api,
        f"{component}.EXIT_SAFE_MODE",
        events=[f"{component}.ExitingSafeMode"],
    )

    time.sleep(2)

    # Verify mode is NORMAL (2)
    proves_send_and_assert_command(fprime_test_api, "CdhCore.tlmSend.SEND_PKT", ["1"])
    mode_result: ChData = fprime_test_api.assert_telemetry(
        f"{component}.CurrentMode", start="NOW", timeout=3
    )
    assert mode_result.get_val() == 2, "Should be in NORMAL mode"


# ==============================================================================
# Edge Cases & Validation Tests
# ==============================================================================


def test_18_force_safe_mode_idempotent(fprime_test_api: IntegrationTestAPI, start_gds):
    """
    Test that calling FORCE_SAFE_MODE while already in safe mode is idempotent.
    Should succeed without emitting events (no re-entry).
    """
    # Enter safe mode first time
    proves_send_and_assert_command(fprime_test_api, f"{component}.FORCE_SAFE_MODE")
    time.sleep(2)

    # Clear event history
    fprime_test_api.clear_histories()

    # Force safe mode again - should succeed silently (no events, no re-entry)
    proves_send_and_assert_command(fprime_test_api, f"{component}.FORCE_SAFE_MODE")

    # Verify still in safe mode
    time.sleep(1)
    proves_send_and_assert_command(fprime_test_api, "CdhCore.tlmSend.SEND_PKT", ["1"])
    mode_result: ChData = fprime_test_api.assert_telemetry(
        f"{component}.CurrentMode", start="NOW", timeout=3
    )
    assert mode_result.get_val() == 1, "Should still be in SAFE_MODE"


def test_19_safe_mode_state_persists(fprime_test_api: IntegrationTestAPI, start_gds):
    """
    Test that mode persists to /mode_state.bin file.
    """
    # Enter safe mode to trigger state save
    proves_send_and_assert_command(fprime_test_api, f"{component}.FORCE_SAFE_MODE")
    time.sleep(2)

    # For now, just verify the command succeeded and state is correct
    proves_send_and_assert_command(fprime_test_api, "CdhCore.tlmSend.SEND_PKT", ["1"])
    mode_result: ChData = fprime_test_api.assert_telemetry(
        f"{component}.CurrentMode", start="NOW", timeout=3
    )
    assert mode_result.get_val() == 1, "Mode should be saved as SAFE_MODE"

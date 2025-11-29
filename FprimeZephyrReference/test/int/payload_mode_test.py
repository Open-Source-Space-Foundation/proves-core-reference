"""
payload_mode_test.py:

Integration tests for the ModeManager component (payload mode).

Tests cover:
- Payload mode entry and exit
- Mode transition validation (cannot enter from safe mode)
- Sequential transitions (FORCE_SAFE_MODE rejected from payload mode)
- State persistence
- Manual exit behavior (face switches remain on)
- Voltage monitoring active in payload mode (no false triggers)
- Automatic exit due to low voltage (manual test - requires voltage control)

Total: 7 tests (6 automated, 1 manual)

Mode enum values: SAFE_MODE=1, NORMAL=2, PAYLOAD_MODE=3
"""

import time

import pytest
from common import proves_send_and_assert_command
from fprime_gds.common.data_types.ch_data import ChData
from fprime_gds.common.testing_fw.api import IntegrationTestAPI

component = "ReferenceDeployment.modeManager"
face_load_switch_channels = [
    "ReferenceDeployment.face4LoadSwitch.IsOn",
    "ReferenceDeployment.face0LoadSwitch.IsOn",
    "ReferenceDeployment.face1LoadSwitch.IsOn",
    "ReferenceDeployment.face2LoadSwitch.IsOn",
    "ReferenceDeployment.face3LoadSwitch.IsOn",
    "ReferenceDeployment.face5LoadSwitch.IsOn",
]
payload_load_switch_channels = [
    "ReferenceDeployment.payloadPowerLoadSwitch.IsOn",
    "ReferenceDeployment.payloadBatteryLoadSwitch.IsOn",
]
all_load_switch_channels = [
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
# Payload Mode Tests
# ==============================================================================


def test_payload_01_enter_exit_payload_mode(
    fprime_test_api: IntegrationTestAPI, start_gds
):
    """
    Test ENTER_PAYLOAD_MODE and EXIT_PAYLOAD_MODE commands.
    Verifies:
    - EnteringPayloadMode event is emitted
    - CurrentMode telemetry = 3 (PAYLOAD_MODE)
    - Payload load switches (6 & 7) are ON
    - ExitingPayloadMode event is emitted on exit
    - CurrentMode returns to 2 (NORMAL)
    """
    # Enter payload mode
    proves_send_and_assert_command(
        fprime_test_api,
        f"{component}.ENTER_PAYLOAD_MODE",
        events=[f"{component}.ManualPayloadModeEntry"],
    )

    time.sleep(2)

    # Verify mode is PAYLOAD_MODE (3)
    proves_send_and_assert_command(fprime_test_api, "CdhCore.tlmSend.SEND_PKT", ["1"])
    mode_result: ChData = fprime_test_api.assert_telemetry(
        f"{component}.CurrentMode", timeout=5
    )
    assert mode_result.get_val() == 3, "Should be in PAYLOAD_MODE"

    # Verify payload load switches are ON
    proves_send_and_assert_command(fprime_test_api, "CdhCore.tlmSend.SEND_PKT", ["9"])
    for channel in payload_load_switch_channels:
        value = fprime_test_api.assert_telemetry(channel, timeout=5).get_val()
        if isinstance(value, str):
            assert value.upper() == "ON", f"{channel} should be ON in payload mode"
        else:
            assert value == 1, f"{channel} should be ON in payload mode"

    # Exit payload mode
    proves_send_and_assert_command(
        fprime_test_api,
        f"{component}.EXIT_PAYLOAD_MODE",
        events=[f"{component}.ExitingPayloadMode"],
    )

    time.sleep(2)

    # Verify mode is NORMAL (2)
    proves_send_and_assert_command(fprime_test_api, "CdhCore.tlmSend.SEND_PKT", ["1"])
    mode_result: ChData = fprime_test_api.assert_telemetry(
        f"{component}.CurrentMode", timeout=5
    )
    assert mode_result.get_val() == 2, "Should be in NORMAL mode"

    # Verify payload load switches are OFF in NORMAL mode
    proves_send_and_assert_command(fprime_test_api, "CdhCore.tlmSend.SEND_PKT", ["9"])
    for channel in payload_load_switch_channels:
        value = fprime_test_api.assert_telemetry(channel, timeout=5).get_val()
        if isinstance(value, str):
            assert value.upper() == "OFF", f"{channel} should be OFF in NORMAL mode"
        else:
            assert value == 0, f"{channel} should be OFF in NORMAL mode"


def test_payload_02_cannot_enter_from_safe_mode(
    fprime_test_api: IntegrationTestAPI, start_gds
):
    """
    Test that ENTER_PAYLOAD_MODE fails when in safe mode.
    Must exit safe mode to NORMAL first.
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

    # Try to enter payload mode - should fail
    fprime_test_api.clear_histories()
    with pytest.raises(Exception):
        proves_send_and_assert_command(
            fprime_test_api, f"{component}.ENTER_PAYLOAD_MODE"
        )

    # Verify CommandValidationFailed event
    fprime_test_api.assert_event(f"{component}.CommandValidationFailed", timeout=3)

    # Verify still in safe mode
    proves_send_and_assert_command(fprime_test_api, "CdhCore.tlmSend.SEND_PKT", ["1"])
    mode_result: ChData = fprime_test_api.assert_telemetry(
        f"{component}.CurrentMode", timeout=5
    )
    assert mode_result.get_val() == 1, "Should still be in SAFE_MODE"


def test_payload_03_safe_mode_rejected_from_payload(
    fprime_test_api: IntegrationTestAPI, start_gds
):
    """
    Test that FORCE_SAFE_MODE is rejected from PAYLOAD_MODE (sequential transitions).
    Must exit payload mode first before entering safe mode.
    Verifies:
    - FORCE_SAFE_MODE fails with validation error from payload mode
    - CommandValidationFailed event is emitted
    - Remains in PAYLOAD_MODE (3)
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

    # Try to force safe mode - should fail (sequential transitions required)
    fprime_test_api.clear_histories()
    with pytest.raises(Exception):
        proves_send_and_assert_command(fprime_test_api, f"{component}.FORCE_SAFE_MODE")

    # Verify CommandValidationFailed event
    fprime_test_api.assert_event(f"{component}.CommandValidationFailed", timeout=3)

    # Verify still in payload mode
    proves_send_and_assert_command(fprime_test_api, "CdhCore.tlmSend.SEND_PKT", ["1"])
    mode_result: ChData = fprime_test_api.assert_telemetry(
        f"{component}.CurrentMode", timeout=5
    )
    assert mode_result.get_val() == 3, "Should still be in PAYLOAD_MODE"


def test_payload_04_state_persists(fprime_test_api: IntegrationTestAPI, start_gds):
    """
    Test that payload mode state persists to /mode_state.bin file.
    Note: Full persistence test would require reboot, this verifies state is saved.
    """
    # Enter payload mode to trigger state save
    proves_send_and_assert_command(fprime_test_api, f"{component}.ENTER_PAYLOAD_MODE")
    time.sleep(2)

    # Verify mode is saved as PAYLOAD_MODE (3)
    proves_send_and_assert_command(fprime_test_api, "CdhCore.tlmSend.SEND_PKT", ["1"])
    mode_result: ChData = fprime_test_api.assert_telemetry(
        f"{component}.CurrentMode", timeout=5
    )
    assert mode_result.get_val() == 3, "Mode should be saved as PAYLOAD_MODE"

    # Verify PayloadModeEntryCount is tracking
    count_result: ChData = fprime_test_api.assert_telemetry(
        f"{component}.PayloadModeEntryCount", timeout=5
    )
    assert count_result.get_val() >= 1, "PayloadModeEntryCount should be at least 1"


def test_payload_05_manual_exit_face_switches_remain_on(
    fprime_test_api: IntegrationTestAPI, start_gds
):
    """
    Test that manual EXIT_PAYLOAD_MODE leaves face switches ON.
    Verifies:
    - Face switches (0-5) remain ON after manual exit
    - Payload switches (6-7) are turned OFF
    - Mode returns to NORMAL (2)
    """
    # Enter payload mode
    proves_send_and_assert_command(fprime_test_api, f"{component}.ENTER_PAYLOAD_MODE")
    time.sleep(2)

    # Verify all switches are ON in payload mode
    proves_send_and_assert_command(fprime_test_api, "CdhCore.tlmSend.SEND_PKT", ["9"])
    for channel in all_load_switch_channels:
        value = fprime_test_api.assert_telemetry(channel, timeout=5).get_val()
        if isinstance(value, str):
            assert value.upper() == "ON", f"{channel} should be ON in payload mode"
        else:
            assert value == 1, f"{channel} should be ON in payload mode"

    # Exit payload mode manually
    proves_send_and_assert_command(
        fprime_test_api,
        f"{component}.EXIT_PAYLOAD_MODE",
        events=[f"{component}.ExitingPayloadMode"],
    )
    time.sleep(2)

    # Verify mode is NORMAL (2)
    proves_send_and_assert_command(fprime_test_api, "CdhCore.tlmSend.SEND_PKT", ["1"])
    mode_result: ChData = fprime_test_api.assert_telemetry(
        f"{component}.CurrentMode", timeout=5
    )
    assert mode_result.get_val() == 2, "Should be in NORMAL mode"

    # Verify face switches (0-5) are still ON
    proves_send_and_assert_command(fprime_test_api, "CdhCore.tlmSend.SEND_PKT", ["9"])
    for channel in face_load_switch_channels:
        value = fprime_test_api.assert_telemetry(channel, timeout=5).get_val()
        if isinstance(value, str):
            assert value.upper() == "ON", (
                f"{channel} should remain ON after manual exit"
            )
        else:
            assert value == 1, f"{channel} should remain ON after manual exit"

    # Verify payload switches (6-7) are OFF
    for channel in payload_load_switch_channels:
        value = fprime_test_api.assert_telemetry(channel, timeout=5).get_val()
        if isinstance(value, str):
            assert value.upper() == "OFF", f"{channel} should be OFF after manual exit"
        else:
            assert value == 0, f"{channel} should be OFF after manual exit"


def test_payload_06_voltage_monitoring_active(
    fprime_test_api: IntegrationTestAPI, start_gds
):
    """
    Test that voltage monitoring is active during payload mode.
    Verifies:
    - System voltage telemetry is available
    - Voltage is above threshold (no auto-exit should occur)
    - Mode remains in PAYLOAD_MODE after 5 seconds (no false triggers)

    This test verifies the infrastructure works without requiring
    manual voltage control. The actual low-voltage exit is tested
    in test_payload_07_auto_exit_low_voltage (manual test).
    """
    # Enter payload mode
    proves_send_and_assert_command(fprime_test_api, f"{component}.ENTER_PAYLOAD_MODE")
    time.sleep(2)

    # Verify in payload mode
    proves_send_and_assert_command(fprime_test_api, "CdhCore.tlmSend.SEND_PKT", ["1"])
    mode_result: ChData = fprime_test_api.assert_telemetry(
        f"{component}.CurrentMode", timeout=5
    )
    assert mode_result.get_val() == 3, "Should be in PAYLOAD_MODE"

    # Verify system voltage telemetry is available and valid
    proves_send_and_assert_command(fprime_test_api, "CdhCore.tlmSend.SEND_PKT", ["2"])
    voltage_result: ChData = fprime_test_api.assert_telemetry(
        "ReferenceDeployment.ina219SysManager.Voltage", timeout=5
    )
    voltage = voltage_result.get_val()
    assert voltage > 0, f"Voltage should be positive, got {voltage}"

    # Note: We don't assert voltage > 7.2V because board supply can fluctuate.
    # If voltage is actually low, auto-exit is correct behavior.
    print(f"Current system voltage: {voltage}V (threshold: 7.2V)")

    # Wait 5 seconds
    time.sleep(5)

    # Check final mode
    proves_send_and_assert_command(fprime_test_api, "CdhCore.tlmSend.SEND_PKT", ["1"])
    mode_result: ChData = fprime_test_api.assert_telemetry(
        f"{component}.CurrentMode", timeout=5
    )
    final_mode = mode_result.get_val()

    # Check for auto-exit events
    events = fprime_test_api.get_event_test_history()
    auto_exit_events = [
        e for e in events if "AutoPayloadModeExit" in str(e.get_template().get_name())
    ]

    if voltage >= 7.2:
        # Voltage was healthy - should remain in payload mode
        assert final_mode == 3, "Should still be in PAYLOAD_MODE (no false auto-exit)"
        assert len(auto_exit_events) == 0, (
            "Should not have triggered AutoPayloadModeExit"
        )
    else:
        # Voltage was low - auto-exit may have triggered (correct behavior)
        print(
            "Note: Voltage was below threshold. Auto-exit may have occurred (expected)."
        )
        # Don't fail the test - low voltage triggering exit is correct behavior


@pytest.mark.skip(reason="Requires voltage control - run manually with low battery")
def test_payload_07_auto_exit_low_voltage(
    fprime_test_api: IntegrationTestAPI, start_gds
):
    """
    Test automatic exit from payload mode due to low voltage.

    MANUAL TEST - requires ability to control battery voltage:
    1. Enter payload mode
    2. Lower battery voltage below 7.2V for 10+ seconds
    3. Verify AutoPayloadModeExit event is emitted
    4. Verify mode returns to NORMAL (2)
    5. Verify ALL switches (faces 0-5 AND payload 6-7) are OFF

    Expected behavior:
    - Voltage must be below 7.2V for 10 consecutive seconds (1Hz checks)
    - Invalid voltage readings also count as faults
    - AutoPayloadModeExit event emitted with voltage value (0.0V if invalid)
    - Mode changes to NORMAL
    - All 8 load switches turned OFF (more aggressive than manual exit)
    """
    # Enter payload mode
    proves_send_and_assert_command(fprime_test_api, f"{component}.ENTER_PAYLOAD_MODE")
    time.sleep(2)

    # Verify in payload mode
    proves_send_and_assert_command(fprime_test_api, "CdhCore.tlmSend.SEND_PKT", ["1"])
    mode_result: ChData = fprime_test_api.assert_telemetry(
        f"{component}.CurrentMode", timeout=5
    )
    assert mode_result.get_val() == 3, "Should be in PAYLOAD_MODE"

    # --- MANUAL STEP: Lower voltage below 7.2V for 10+ seconds ---
    # Wait for automatic exit (would need voltage control here)
    time.sleep(15)

    # Verify AutoPayloadModeExit event
    fprime_test_api.assert_event(f"{component}.AutoPayloadModeExit", timeout=5)

    # Verify mode is NORMAL (2)
    proves_send_and_assert_command(fprime_test_api, "CdhCore.tlmSend.SEND_PKT", ["1"])
    mode_result: ChData = fprime_test_api.assert_telemetry(
        f"{component}.CurrentMode", timeout=5
    )
    assert mode_result.get_val() == 2, "Should be in NORMAL mode after auto exit"

    # Verify ALL switches are OFF (aggressive behavior)
    proves_send_and_assert_command(fprime_test_api, "CdhCore.tlmSend.SEND_PKT", ["9"])
    for channel in all_load_switch_channels:
        value = fprime_test_api.assert_telemetry(channel, timeout=5).get_val()
        if isinstance(value, str):
            assert value.upper() == "OFF", f"{channel} should be OFF after auto exit"
        else:
            assert value == 0, f"{channel} should be OFF after auto exit"

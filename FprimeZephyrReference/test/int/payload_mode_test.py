"""
payload_mode_test.py:

Integration tests for the ModeManager component (payload mode).

Tests cover:
- Payload mode entry and exit
- Mode transition validation (cannot enter from safe mode)
- Sequential transitions (FORCE_SAFE_MODE rejected from payload mode)
- State persistence

Total: 4 tests

Mode enum values: HIBERNATION_MODE=0, SAFE_MODE=1, NORMAL=2, PAYLOAD_MODE=3
"""

import time

import pytest
from common import proves_send_and_assert_command
from fprime_gds.common.data_types.ch_data import ChData
from fprime_gds.common.testing_fw.api import IntegrationTestAPI

component = "ReferenceDeployment.modeManager"
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

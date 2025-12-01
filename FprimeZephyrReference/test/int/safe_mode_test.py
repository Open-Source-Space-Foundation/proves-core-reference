"""
safe_mode_test.py:

Integration tests for the ModeManager component (Normal <-> Safe mode transitions).

Tests cover:
- CurrentSafeModeReason telemetry verification
- Safe mode reason tracking (GROUND_COMMAND, LOW_BATTERY, SYSTEM_FAULT)
- Auto safe mode entry due to low voltage (manual test - requires voltage control)
- Auto safe mode exit/recovery when voltage > 8V (manual test - requires voltage control)
- Unintended reboot detection (manual test - requires reboot)

SafeModeReason Test Coverage:
| Reason           | Value | Test                    | Status                          |
|------------------|-------|-------------------------|---------------------------------|
| NONE             | 0     | test_safe_01, 03        | ✅ Automated                    |
| LOW_BATTERY      | 1     | test_safe_05, 06        | ⏸️ Manual (voltage control)     |
| SYSTEM_FAULT     | 2     | test_safe_07            | ⏸️ Manual (power cycle)         |
| GROUND_COMMAND   | 3     | test_safe_02, 04        | ✅ Automated                    |
| EXTERNAL_REQUEST | 4     | N/A                     | ❌ Internal port only           |
| LORA             | 5     | N/A                     | ❌ Internal port only           |

Note: EXTERNAL_REQUEST is triggered via forceSafeMode port (used by other components).
      LORA is triggered by LoRa driver when communication timeout/fault occurs.
      These cannot be tested via ground commands in integration tests.

Total: 8 tests (4 automated, 4 manual)

SafeModeReason enum values: NONE=0, LOW_BATTERY=1, SYSTEM_FAULT=2, GROUND_COMMAND=3, EXTERNAL_REQUEST=4, LORA=5
Mode enum values: SAFE_MODE=1, NORMAL=2
"""

import logging
import time

import pytest
from common import proves_send_and_assert_command
from fprime_gds.common.data_types.ch_data import ChData
from fprime_gds.common.testing_fw.api import IntegrationTestAPI

logger = logging.getLogger(__name__)

component = "ReferenceDeployment.modeManager"

# Telemetry packet IDs for CdhCore.tlmSend.SEND_PKT command
MODE_TELEMETRY_PACKET_ID = (
    "1"  # ModeManager telemetry (CurrentMode, SafeModeReason, etc.)
)
VOLTAGE_TELEMETRY_PACKET_ID = "10"  # PowerMonitor telemetry (includes INA219 voltage)

# Voltage thresholds (must match ModeManager.hpp constants)
SAFE_MODE_ENTRY_VOLTAGE = 6.7  # Volts - threshold for entering safe mode from NORMAL
SAFE_MODE_RECOVERY_VOLTAGE = 8.0  # Volts - threshold for auto-recovery from safe mode
SAFE_MODE_DEBOUNCE_SECONDS = 10  # Consecutive seconds for safe mode transitions

# SafeModeReason enum values (must match ModeManager.fpp)
SAFE_MODE_REASON_NONE = 0
SAFE_MODE_REASON_LOW_BATTERY = 1
SAFE_MODE_REASON_SYSTEM_FAULT = 2
SAFE_MODE_REASON_GROUND_COMMAND = 3
SAFE_MODE_REASON_EXTERNAL_REQUEST = 4
SAFE_MODE_REASON_LORA = 5


@pytest.fixture(autouse=True)
def setup_and_teardown(fprime_test_api: IntegrationTestAPI, start_gds):
    """
    Setup before each test and cleanup after.
    Ensures clean state by exiting safe mode if needed.
    """
    # Setup: Try to get to a clean NORMAL state
    try:
        proves_send_and_assert_command(fprime_test_api, f"{component}.EXIT_SAFE_MODE")
    except Exception as e:
        logger.debug(f"Setup: EXIT_SAFE_MODE skipped (not in safe mode): {e}")

    # Clear event and telemetry history before test
    fprime_test_api.clear_histories()

    yield

    # Teardown: Return to clean NORMAL state for next test
    try:
        proves_send_and_assert_command(fprime_test_api, f"{component}.EXIT_SAFE_MODE")
    except Exception as e:
        logger.debug(f"Teardown: EXIT_SAFE_MODE skipped (not in safe mode): {e}")


# ==============================================================================
# SafeModeReason Telemetry Tests
# ==============================================================================


def test_safe_01_initial_safe_mode_reason_is_none(
    fprime_test_api: IntegrationTestAPI, start_gds
):
    """
    Test that CurrentSafeModeReason telemetry is NONE when not in safe mode.
    """
    # Ensure we're in NORMAL mode
    proves_send_and_assert_command(
        fprime_test_api, "CdhCore.tlmSend.SEND_PKT", [MODE_TELEMETRY_PACKET_ID]
    )
    mode_result: ChData = fprime_test_api.assert_telemetry(
        f"{component}.CurrentMode", timeout=5
    )

    if mode_result.get_val() != 2:
        # Not in NORMAL mode, skip test
        pytest.skip("Not in NORMAL mode - cannot verify initial reason")

    # Verify safe mode reason is NONE
    reason_result: ChData = fprime_test_api.assert_telemetry(
        f"{component}.CurrentSafeModeReason", timeout=5
    )
    reason_val = reason_result.get_val()

    # Handle both numeric and string representations
    if isinstance(reason_val, str):
        assert "NONE" in reason_val.upper(), (
            f"Safe mode reason should be NONE, got {reason_val}"
        )
    else:
        assert reason_val == SAFE_MODE_REASON_NONE, (
            f"Safe mode reason should be NONE (0), got {reason_val}"
        )


def test_safe_02_ground_command_sets_reason(
    fprime_test_api: IntegrationTestAPI, start_gds
):
    """
    Test that FORCE_SAFE_MODE command sets reason to GROUND_COMMAND.
    Verifies:
    - CurrentSafeModeReason = GROUND_COMMAND after command
    - EnteringSafeMode event contains "Ground command"
    """
    # Enter safe mode via ground command
    fprime_test_api.clear_histories()
    proves_send_and_assert_command(
        fprime_test_api,
        f"{component}.FORCE_SAFE_MODE",
        events=[f"{component}.ManualSafeModeEntry"],
    )
    time.sleep(2)

    # Verify safe mode reason is GROUND_COMMAND
    proves_send_and_assert_command(
        fprime_test_api, "CdhCore.tlmSend.SEND_PKT", [MODE_TELEMETRY_PACKET_ID]
    )
    reason_result: ChData = fprime_test_api.assert_telemetry(
        f"{component}.CurrentSafeModeReason", timeout=5
    )
    reason_val = reason_result.get_val()

    if isinstance(reason_val, str):
        assert "GROUND_COMMAND" in reason_val.upper(), (
            f"Safe mode reason should be GROUND_COMMAND, got {reason_val}"
        )
    else:
        assert reason_val == SAFE_MODE_REASON_GROUND_COMMAND, (
            f"Safe mode reason should be GROUND_COMMAND (3), got {reason_val}"
        )

    # Verify EnteringSafeMode event mentions ground command
    events = fprime_test_api.get_event_test_history()
    entering_events = [
        e for e in events if "EnteringSafeMode" in str(e.get_template().get_name())
    ]
    assert len(entering_events) > 0, "Should have EnteringSafeMode event"
    assert "Ground command" in entering_events[-1].get_display_text(), (
        "EnteringSafeMode reason should mention Ground command"
    )


def test_safe_03_exit_clears_reason(fprime_test_api: IntegrationTestAPI, start_gds):
    """
    Test that EXIT_SAFE_MODE clears the safe mode reason to NONE.
    """
    # Enter safe mode
    proves_send_and_assert_command(fprime_test_api, f"{component}.FORCE_SAFE_MODE")
    time.sleep(2)

    # Verify in safe mode with reason set
    proves_send_and_assert_command(
        fprime_test_api, "CdhCore.tlmSend.SEND_PKT", [MODE_TELEMETRY_PACKET_ID]
    )
    mode_result: ChData = fprime_test_api.assert_telemetry(
        f"{component}.CurrentMode", timeout=5
    )
    assert mode_result.get_val() == 1, "Should be in SAFE_MODE"

    # Exit safe mode
    proves_send_and_assert_command(
        fprime_test_api,
        f"{component}.EXIT_SAFE_MODE",
        events=[f"{component}.ExitingSafeMode"],
    )
    time.sleep(2)

    # Verify reason is cleared to NONE
    proves_send_and_assert_command(
        fprime_test_api, "CdhCore.tlmSend.SEND_PKT", [MODE_TELEMETRY_PACKET_ID]
    )
    reason_result: ChData = fprime_test_api.assert_telemetry(
        f"{component}.CurrentSafeModeReason", timeout=5
    )
    reason_val = reason_result.get_val()

    if isinstance(reason_val, str):
        assert "NONE" in reason_val.upper(), (
            f"Safe mode reason should be NONE after exit, got {reason_val}"
        )
    else:
        assert reason_val == SAFE_MODE_REASON_NONE, (
            f"Safe mode reason should be NONE (0) after exit, got {reason_val}"
        )


def test_safe_04_no_auto_recovery_for_ground_command(
    fprime_test_api: IntegrationTestAPI, start_gds
):
    """
    Test that safe mode entered via GROUND_COMMAND does NOT auto-recover.
    Even if voltage is healthy, must use EXIT_SAFE_MODE command.
    Verifies:
    - Safe mode persists after debounce period + margin
    - No AutoSafeModeExit event is emitted
    """
    # Enter safe mode via ground command
    proves_send_and_assert_command(fprime_test_api, f"{component}.FORCE_SAFE_MODE")
    time.sleep(2)

    # Verify in safe mode with GROUND_COMMAND reason
    proves_send_and_assert_command(
        fprime_test_api, "CdhCore.tlmSend.SEND_PKT", [MODE_TELEMETRY_PACKET_ID]
    )
    mode_result: ChData = fprime_test_api.assert_telemetry(
        f"{component}.CurrentMode", timeout=5
    )
    assert mode_result.get_val() == 1, "Should be in SAFE_MODE"

    fprime_test_api.clear_histories()

    # Wait for debounce period + margin (should NOT auto-recover)
    logger.info(
        f"Waiting {SAFE_MODE_DEBOUNCE_SECONDS + 3} seconds to verify no auto-recovery..."
    )
    time.sleep(SAFE_MODE_DEBOUNCE_SECONDS + 3)

    # Verify still in safe mode
    proves_send_and_assert_command(
        fprime_test_api, "CdhCore.tlmSend.SEND_PKT", [MODE_TELEMETRY_PACKET_ID]
    )
    mode_result: ChData = fprime_test_api.assert_telemetry(
        f"{component}.CurrentMode", timeout=5
    )
    assert mode_result.get_val() == 1, (
        "Should still be in SAFE_MODE (no auto-recovery for GROUND_COMMAND reason)"
    )

    # Verify no AutoSafeModeExit event
    events = fprime_test_api.get_event_test_history()
    auto_exit_events = [
        e for e in events if "AutoSafeModeExit" in str(e.get_template().get_name())
    ]
    assert len(auto_exit_events) == 0, (
        "Should NOT have AutoSafeModeExit event for GROUND_COMMAND reason"
    )


# ==============================================================================
# Manual Tests - Require Voltage Control
# ==============================================================================


@pytest.mark.skip(reason="Requires voltage control - run manually with low battery")
def test_safe_05_auto_entry_low_voltage(fprime_test_api: IntegrationTestAPI, start_gds):
    """
    Test automatic entry into safe mode due to low voltage.

    MANUAL TEST - requires ability to control battery voltage:
    1. Ensure in NORMAL mode
    2. Lower battery voltage below SAFE_MODE_ENTRY_VOLTAGE (6.7V) for SAFE_MODE_DEBOUNCE_SECONDS
    3. Verify AutoSafeModeEntry event is emitted with reason=LOW_BATTERY
    4. Verify mode changes to SAFE_MODE (1)
    5. Verify CurrentSafeModeReason = LOW_BATTERY

    Expected behavior:
    - Voltage must be below 6.7V threshold for 10 consecutive seconds (1Hz checks)
    - Invalid voltage readings also count as faults
    - AutoSafeModeEntry event emitted with reason=LOW_BATTERY and voltage value
    - Mode changes to SAFE_MODE
    - All 8 load switches turned OFF
    """
    # Ensure in NORMAL mode
    proves_send_and_assert_command(
        fprime_test_api, "CdhCore.tlmSend.SEND_PKT", [MODE_TELEMETRY_PACKET_ID]
    )
    mode_result: ChData = fprime_test_api.assert_telemetry(
        f"{component}.CurrentMode", timeout=5
    )
    assert mode_result.get_val() == 2, "Should start in NORMAL mode"

    fprime_test_api.clear_histories()

    # --- MANUAL STEP: Lower voltage below 6.7V for 10+ seconds ---
    logger.info(
        f"MANUAL: Lower voltage below {SAFE_MODE_ENTRY_VOLTAGE}V for {SAFE_MODE_DEBOUNCE_SECONDS}+ seconds"
    )
    time.sleep(SAFE_MODE_DEBOUNCE_SECONDS + 5)

    # Verify AutoSafeModeEntry event
    fprime_test_api.assert_event(f"{component}.AutoSafeModeEntry", timeout=5)

    # Verify mode is SAFE_MODE (1)
    proves_send_and_assert_command(
        fprime_test_api, "CdhCore.tlmSend.SEND_PKT", [MODE_TELEMETRY_PACKET_ID]
    )
    mode_result: ChData = fprime_test_api.assert_telemetry(
        f"{component}.CurrentMode", timeout=5
    )
    assert mode_result.get_val() == 1, "Should be in SAFE_MODE after low voltage"

    # Verify reason is LOW_BATTERY
    reason_result: ChData = fprime_test_api.assert_telemetry(
        f"{component}.CurrentSafeModeReason", timeout=5
    )
    reason_val = reason_result.get_val()
    if isinstance(reason_val, str):
        assert "LOW_BATTERY" in reason_val.upper(), (
            f"Safe mode reason should be LOW_BATTERY, got {reason_val}"
        )
    else:
        assert reason_val == SAFE_MODE_REASON_LOW_BATTERY, (
            f"Safe mode reason should be LOW_BATTERY (1), got {reason_val}"
        )


@pytest.mark.skip(
    reason="Requires voltage control - run manually with battery recovery"
)
def test_safe_06_auto_recovery_voltage(fprime_test_api: IntegrationTestAPI, start_gds):
    """
    Test automatic recovery from safe mode when voltage recovers.

    MANUAL TEST - requires ability to control battery voltage:
    1. Enter safe mode with LOW_BATTERY reason (use test_safe_05 first or simulate)
    2. Raise battery voltage above SAFE_MODE_RECOVERY_VOLTAGE (8.0V) for SAFE_MODE_DEBOUNCE_SECONDS
    3. Verify AutoSafeModeExit event is emitted with recovered voltage
    4. Verify mode changes to NORMAL (2)
    5. Verify CurrentSafeModeReason = NONE

    Expected behavior:
    - Only works if reason is LOW_BATTERY (not SYSTEM_FAULT or GROUND_COMMAND)
    - Voltage must be above 8.0V threshold for 10 consecutive seconds
    - AutoSafeModeExit event emitted with voltage value
    - Mode changes to NORMAL
    - Face switches (0-5) turned ON
    """
    # This test assumes we're in safe mode with LOW_BATTERY reason
    # In real testing, run test_safe_05 first or manually set up the state

    # Verify in safe mode with LOW_BATTERY reason
    proves_send_and_assert_command(
        fprime_test_api, "CdhCore.tlmSend.SEND_PKT", [MODE_TELEMETRY_PACKET_ID]
    )
    mode_result: ChData = fprime_test_api.assert_telemetry(
        f"{component}.CurrentMode", timeout=5
    )
    assert mode_result.get_val() == 1, "Should be in SAFE_MODE"

    reason_result: ChData = fprime_test_api.assert_telemetry(
        f"{component}.CurrentSafeModeReason", timeout=5
    )
    reason_val = reason_result.get_val()
    # Skip if not LOW_BATTERY reason
    if isinstance(reason_val, str):
        if "LOW_BATTERY" not in reason_val.upper():
            pytest.skip(
                "Safe mode reason is not LOW_BATTERY - auto-recovery won't work"
            )
    else:
        if reason_val != SAFE_MODE_REASON_LOW_BATTERY:
            pytest.skip(
                "Safe mode reason is not LOW_BATTERY - auto-recovery won't work"
            )

    fprime_test_api.clear_histories()

    # --- MANUAL STEP: Raise voltage above 8.0V for 10+ seconds ---
    logger.info(
        f"MANUAL: Raise voltage above {SAFE_MODE_RECOVERY_VOLTAGE}V for {SAFE_MODE_DEBOUNCE_SECONDS}+ seconds"
    )
    time.sleep(SAFE_MODE_DEBOUNCE_SECONDS + 5)

    # Verify AutoSafeModeExit event
    fprime_test_api.assert_event(f"{component}.AutoSafeModeExit", timeout=5)

    # Verify mode is NORMAL (2)
    proves_send_and_assert_command(
        fprime_test_api, "CdhCore.tlmSend.SEND_PKT", [MODE_TELEMETRY_PACKET_ID]
    )
    mode_result: ChData = fprime_test_api.assert_telemetry(
        f"{component}.CurrentMode", timeout=5
    )
    assert mode_result.get_val() == 2, "Should be in NORMAL mode after recovery"

    # Verify reason is cleared to NONE
    reason_result: ChData = fprime_test_api.assert_telemetry(
        f"{component}.CurrentSafeModeReason", timeout=5
    )
    reason_val = reason_result.get_val()
    if isinstance(reason_val, str):
        assert "NONE" in reason_val.upper(), (
            f"Safe mode reason should be NONE after recovery, got {reason_val}"
        )
    else:
        assert reason_val == SAFE_MODE_REASON_NONE, (
            f"Safe mode reason should be NONE (0) after recovery, got {reason_val}"
        )


@pytest.mark.skip(reason="Requires reboot - run manually with power cycle")
def test_safe_07_unintended_reboot_detection(
    fprime_test_api: IntegrationTestAPI, start_gds
):
    """
    Test unintended reboot detection and automatic safe mode entry.

    MANUAL TEST - requires power cycle without clean shutdown:
    1. Ensure in NORMAL mode
    2. Power cycle the board WITHOUT using COLD_RESET or WARM_RESET commands
    3. On boot, verify UnintendedRebootDetected event is emitted
    4. Verify mode is SAFE_MODE (1)
    5. Verify CurrentSafeModeReason = SYSTEM_FAULT
    6. Verify auto-recovery does NOT work (must use EXIT_SAFE_MODE)

    Expected behavior:
    - cleanShutdown flag is 0 (not set by prepareForReboot)
    - On boot, ModeManager detects unintended reboot
    - Enters SAFE_MODE with reason = SYSTEM_FAULT
    - Requires manual EXIT_SAFE_MODE to recover (no auto-recovery)
    """
    # This test requires a power cycle, so we can only verify the aftermath

    # Check for UnintendedRebootDetected event (may have been emitted at boot)
    events = fprime_test_api.get_event_test_history()
    unintended_events = [
        e
        for e in events
        if "UnintendedRebootDetected" in str(e.get_template().get_name())
    ]

    if len(unintended_events) == 0:
        pytest.skip("No UnintendedRebootDetected event - board may have booted cleanly")

    # Verify in safe mode
    proves_send_and_assert_command(
        fprime_test_api, "CdhCore.tlmSend.SEND_PKT", [MODE_TELEMETRY_PACKET_ID]
    )
    mode_result: ChData = fprime_test_api.assert_telemetry(
        f"{component}.CurrentMode", timeout=5
    )
    assert mode_result.get_val() == 1, "Should be in SAFE_MODE after unintended reboot"

    # Verify reason is SYSTEM_FAULT
    reason_result: ChData = fprime_test_api.assert_telemetry(
        f"{component}.CurrentSafeModeReason", timeout=5
    )
    reason_val = reason_result.get_val()
    if isinstance(reason_val, str):
        assert "SYSTEM_FAULT" in reason_val.upper(), (
            f"Safe mode reason should be SYSTEM_FAULT, got {reason_val}"
        )
    else:
        assert reason_val == SAFE_MODE_REASON_SYSTEM_FAULT, (
            f"Safe mode reason should be SYSTEM_FAULT (2), got {reason_val}"
        )


@pytest.mark.skip(reason="Requires reboot - run manually to verify clean shutdown")
def test_safe_08_clean_reboot_no_safe_mode(
    fprime_test_api: IntegrationTestAPI, start_gds
):
    """
    Test that intentional reboot (via COLD_RESET/WARM_RESET) does NOT trigger safe mode.

    MANUAL TEST - requires reboot via command:
    1. Ensure in NORMAL mode
    2. Issue COLD_RESET or WARM_RESET command
    3. After reboot, verify NO UnintendedRebootDetected event
    4. Verify mode restored to previous state (NORMAL)
    5. Verify CurrentSafeModeReason = NONE

    Expected behavior:
    - prepareForReboot is called before sys_reboot()
    - cleanShutdown flag is set to 1
    - On boot, ModeManager sees clean shutdown, no safe mode entry
    - Mode restored from persistent state
    """
    # Ensure in NORMAL mode before reboot
    proves_send_and_assert_command(
        fprime_test_api, "CdhCore.tlmSend.SEND_PKT", [MODE_TELEMETRY_PACKET_ID]
    )
    mode_result: ChData = fprime_test_api.assert_telemetry(
        f"{component}.CurrentMode", timeout=5
    )
    assert mode_result.get_val() == 2, "Should be in NORMAL mode before reboot"

    # Issue reboot command (this will disconnect GDS)
    logger.info("Issuing COLD_RESET command - GDS will disconnect")
    fprime_test_api.send_command("ReferenceDeployment.resetManager.COLD_RESET", [])

    # --- MANUAL STEP: Reconnect GDS after reboot ---
    logger.info("MANUAL: Reconnect GDS after board reboots")
    time.sleep(30)  # Wait for reboot

    # After reconnect, verify no unintended reboot detection
    fprime_test_api.clear_histories()
    time.sleep(5)  # Allow time for events to be received

    events = fprime_test_api.get_event_test_history()
    unintended_events = [
        e
        for e in events
        if "UnintendedRebootDetected" in str(e.get_template().get_name())
    ]
    assert len(unintended_events) == 0, (
        "Should NOT have UnintendedRebootDetected event after clean reboot"
    )

    # Verify in NORMAL mode
    proves_send_and_assert_command(
        fprime_test_api, "CdhCore.tlmSend.SEND_PKT", [MODE_TELEMETRY_PACKET_ID]
    )
    mode_result: ChData = fprime_test_api.assert_telemetry(
        f"{component}.CurrentMode", timeout=5
    )
    assert mode_result.get_val() == 2, "Should be in NORMAL mode after clean reboot"

    # Verify reason is NONE
    reason_result: ChData = fprime_test_api.assert_telemetry(
        f"{component}.CurrentSafeModeReason", timeout=5
    )
    reason_val = reason_result.get_val()
    if isinstance(reason_val, str):
        assert "NONE" in reason_val.upper(), (
            f"Safe mode reason should be NONE after clean reboot, got {reason_val}"
        )
    else:
        assert reason_val == SAFE_MODE_REASON_NONE, (
            f"Safe mode reason should be NONE (0) after clean reboot, got {reason_val}"
        )

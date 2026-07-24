"""
rf_profile_matrix_test.py:

Integration tests for the RF mode/profile matrix (HIL regression plan rung 6).

Codifies the Phase A bench evidence (logs/HIL-REGRESSION-REPORT-phaseA.md):
  1. SET_TX_PROFILE / SET_RX_PROFILE sweep across the profile set and back,
     asserting each switch completes and the radio stays commandable.
  2. CONTINUOUS_WAVE with clean restore to RX (complements the issue-#207
     regression test in radio_test.py by proving the RX path re-arms —
     a profile switch after CW exercises stop→reconfig→re-arm).
  3. Profile-switch-as-first-post-wake command — the exact SX126x wakeup-race
     shape fixed by the busy-race settle delay (carried patch 0006): idle,
     then issue a profile switch as the first command, repeated N times.
  4. Post-idle (>=90 s) profile switch sanity.
  5. Two-board profile-pairing tests (marked two_board_rf): bidirectional
     frame delivery at each TX/RX profile pairing against a ground radio
     running GRC-USP firmware.  Skipped unless the ground-side environment
     hooks are configured (see below).

Runtime knobs (env vars; defaults are CI-sane, raise them for hammer runs):
  RF_PROFILE_HAMMER_CYCLES  post-wake switch cycles (default 5; bench hammer 100)
  RF_PROFILE_WAKE_IDLE_S    idle seconds before each post-wake switch (default 3)
  RF_PROFILE_LONG_IDLE_S    idle seconds for the post-idle sanity test (default 90)
  USP_GROUND_DATA_TTY       ground radio data-CDC device; downlinked RF frames
                            appear here as raw bytes (e.g. /dev/cu.usbmodem21103)
  USP_GROUND_CMD            shell command template to set the ground radio RX
                            profile; "{profile}" is replaced with the numeric
                            LinkProfileId (e.g. a script wrapping fprime-cli
                            against the ground GDS)
  USP_GROUND_UPLINK_CMD     shell command that makes the ground radio transmit
                            at least one RF frame at the profile whose numeric
                            LinkProfileId replaces "{profile}" (e.g. a script
                            that sets the ground TX profile then sends a bypass
                            NO_OP through a GDS attached to the ground data CDC)
"""

import os
import subprocess
import time
from datetime import datetime

import pytest
from common import cmdDispatch, proves_send_and_assert_command
from fprime_gds.common.models.serialize.time_type import TimeType
from fprime_gds.common.testing_fw.api import IntegrationTestAPI

# Profile switches sever any in-flight RF link (stop→reconfig→re-arm), so this
# whole module must only run when the GDS is connected via UART.
pytestmark = [pytest.mark.uart_only]

downlinkDelay = "ReferenceDeployment.downlinkDelay"
radio = "ReferenceDeployment.uspRadio"

# Events that indicate the radio rejected or failed a reconfiguration.
PROFILE_ERROR_EVENTS = ("ConfigurationFailed", "InvalidProfile")
RADIO_ERROR_EVENTS = ("SendFailed", "ConfigurationFailed", "AllocationFailed")

# LinkProfileId sweep order (P0 is the boot default, so the sweep ends by
# restoring it).  Matches the Phase A rung-6 pairing order.
# P4_GFSK_75K is excluded: not yet exercised on the bench baseline.
PROFILE_SWEEP = ["P1_LORA_SF10", "P2_LORA_SF5", "P3_GFSK_38K", "P0_LORA_SF8"]
BOOT_PROFILE = "P0_LORA_SF8"

# Numeric LinkProfileId values for the ground-side command template.
PROFILE_IDS = {
    "P0_LORA_SF8": 0,
    "P1_LORA_SF10": 1,
    "P2_LORA_SF5": 2,
    "P3_GFSK_38K": 3,
}

# Continuous-wave burst duration (seconds); kept short so the command stays
# within the GDS command-completion timeout (same rationale as radio_test.py).
CW_SECONDS = 5

# RX auto-revert is disabled (revert_s=0) for all single-board switches — the
# tests restore P0 explicitly, and an auto-revert mid-test would race the
# ProfileChanged assertions.
NO_REVERT = 0

HAMMER_CYCLES = int(os.environ.get("RF_PROFILE_HAMMER_CYCLES", "5"))
WAKE_IDLE_S = float(os.environ.get("RF_PROFILE_WAKE_IDLE_S", "3"))
LONG_IDLE_S = float(os.environ.get("RF_PROFILE_LONG_IDLE_S", "90"))

GROUND_DATA_TTY = os.environ.get("USP_GROUND_DATA_TTY")
GROUND_CMD = os.environ.get("USP_GROUND_CMD")
GROUND_UPLINK_CMD = os.environ.get("USP_GROUND_UPLINK_CMD")

GROUND_READ_WINDOW_S = 20.0


def _now_start() -> TimeType:
    return TimeType().set_datetime(
        datetime.now(), time_base=TimeType.TimeBase("TB_DONT_CARE")
    )


def _assert_no_profile_errors(
    fprime_test_api: IntegrationTestAPI, start: TimeType, context: str
) -> None:
    for evt in PROFILE_ERROR_EVENTS:
        result = fprime_test_api.await_event(f"{radio}.{evt}", start=start, timeout=0)
        assert result is None, f"Unexpected {radio}.{evt} {context}: {result}"


def _switch_profile(
    fprime_test_api: IntegrationTestAPI, direction: str, profile: str
) -> None:
    """Issue a SET_TX_PROFILE/SET_RX_PROFILE and assert the deferred apply
    completed cleanly (ProfileChanged emitted, no error events)."""
    start = _now_start()
    if direction == "TX":
        proves_send_and_assert_command(
            fprime_test_api, f"{radio}.SET_TX_PROFILE", [profile]
        )
    else:
        proves_send_and_assert_command(
            fprime_test_api, f"{radio}.SET_RX_PROFILE", [profile, NO_REVERT]
        )
    # The command handler defers the apply to the component thread; the switch
    # is only complete once ProfileChanged is emitted.
    result = fprime_test_api.await_event(
        f"{radio}.ProfileChanged", start=start, timeout=10
    )
    assert result is not None, (
        f"No {radio}.ProfileChanged after SET_{direction}_PROFILE({profile})"
    )
    _assert_no_profile_errors(
        fprime_test_api, start, f"after SET_{direction}_PROFILE({profile})"
    )


@pytest.fixture(autouse=True)
def setup_test(fprime_test_api: IntegrationTestAPI, start_gds):
    """Set the downlink divider before each test; after each test disable
    transmit and restore both profiles to the boot default so no test (or the
    RF link of a subsequent session) inherits an off-default configuration."""
    proves_send_and_assert_command(
        fprime_test_api,
        f"{downlinkDelay}.DIVIDER_PRM_SET",
        [20],
    )
    yield
    proves_send_and_assert_command(
        fprime_test_api,
        f"{radio}.TRANSMIT",
        ["DISABLED"],
    )
    proves_send_and_assert_command(
        fprime_test_api, f"{radio}.SET_TX_PROFILE", [BOOT_PROFILE]
    )
    proves_send_and_assert_command(
        fprime_test_api, f"{radio}.SET_RX_PROFILE", [BOOT_PROFILE, NO_REVERT]
    )


def test_01_tx_profile_sweep(fprime_test_api: IntegrationTestAPI, start_gds):
    """SET_TX_PROFILE sweep P0→P1→P2→P3 and back to P0: every switch must
    complete with ProfileChanged and no error events, and the radio must stay
    commandable throughout (rung-6 single-board half)."""
    for profile in PROFILE_SWEEP:
        _switch_profile(fprime_test_api, "TX", profile)
        # Radio (and command path) still alive after the switch.
        proves_send_and_assert_command(fprime_test_api, f"{cmdDispatch}.CMD_NO_OP")


def test_02_rx_profile_sweep(fprime_test_api: IntegrationTestAPI, start_gds):
    """SET_RX_PROFILE sweep P0→P1→P2→P3 and back to P0.  Each switch is a full
    stop→reconfig→re-arm of the receiver; all must complete cleanly with the
    board still commandable."""
    for profile in PROFILE_SWEEP:
        _switch_profile(fprime_test_api, "RX", profile)
        proves_send_and_assert_command(fprime_test_api, f"{cmdDispatch}.CMD_NO_OP")


def test_03_continuous_wave_restore_to_rx(
    fprime_test_api: IntegrationTestAPI, start_gds
):
    """CONTINUOUS_WAVE on/off with clean restore to RX.

    Complements radio_test.py::test_02_continuous_wave_repeated (issue #207,
    which proves repeated CW and a TRANSMIT afterwards): here the post-CW
    assertion is that the *receive* chain is healthy — an RX profile switch
    (stop→reconfig→re-arm of the receiver) must succeed after the CW teardown,
    which fails if CW left the modem wedged or the RX path un-armed.
    """
    proves_send_and_assert_command(
        fprime_test_api,
        f"{radio}.CONTINUOUS_WAVE",
        [CW_SECONDS],
    )
    # Wait out the CW duration so the asynchronous teardown (restore to RX)
    # completes before probing the receiver.
    time.sleep(CW_SECONDS + 2)

    # Receiver must be reconfigurable post-CW: exercise a full RX
    # stop→reconfig→re-arm cycle away from and back to the boot profile.
    _switch_profile(fprime_test_api, "RX", "P2_LORA_SF5")
    _switch_profile(fprime_test_api, "RX", BOOT_PROFILE)

    # And the TX path must be intact too (no wedge): enabling transmit must
    # produce no radio error events.
    start = _now_start()
    proves_send_and_assert_command(fprime_test_api, f"{radio}.TRANSMIT", ["ENABLED"])
    time.sleep(10)
    for evt in RADIO_ERROR_EVENTS:
        result = fprime_test_api.await_event(f"{radio}.{evt}", start=start, timeout=0)
        assert result is None, f"Unexpected {radio}.{evt} after CW restore: {result}"


def test_04_post_wake_profile_switch_hammer(
    fprime_test_api: IntegrationTestAPI, start_gds
):
    """Profile switch as the first post-wake command (SX126x wakeup-race shape,
    carried patch 0006).  Idle long enough for the radio to sleep, then issue a
    profile switch as the first command; repeat RF_PROFILE_HAMMER_CYCLES times
    alternating P2↔P0 (the Phase A rung-4 hammer pattern, which scored 100/100
    on the fixed build vs ~40% first-command drops before the fix).

    Default cycle count is small for CI; set RF_PROFILE_HAMMER_CYCLES=100 (and
    optionally RF_PROFILE_WAKE_IDLE_S) for a bench hammer run.
    """
    for cycle in range(HAMMER_CYCLES):
        # Idle window: no commands, letting the modem reach its sleep state so
        # the switch below is the first post-wake SPI command sequence.
        time.sleep(WAKE_IDLE_S)
        profile = "P2_LORA_SF5" if cycle % 2 == 0 else BOOT_PROFILE
        try:
            _switch_profile(fprime_test_api, "RX", profile)
        except AssertionError as exc:
            raise AssertionError(
                f"Post-wake profile switch failed on cycle {cycle + 1}/"
                f"{HAMMER_CYCLES}: {exc}"
            ) from exc


def test_05_post_idle_profile_switch(fprime_test_api: IntegrationTestAPI, start_gds):
    """Post-idle profile-switch sanity (Phase A rung-6 idle check): with
    transmit disabled, stay command-idle for RF_PROFILE_LONG_IDLE_S (default
    90 s), then issue a profile switch as the first command and verify the
    board is fully commandable afterwards."""
    proves_send_and_assert_command(fprime_test_api, f"{radio}.TRANSMIT", ["DISABLED"])
    time.sleep(LONG_IDLE_S)
    _switch_profile(fprime_test_api, "RX", "P1_LORA_SF10")
    _switch_profile(fprime_test_api, "RX", BOOT_PROFILE)
    proves_send_and_assert_command(fprime_test_api, f"{cmdDispatch}.CMD_NO_OP")


# ---------------------------------------------------------------------------
# Two-board tests: require a ground radio (GRC-USP firmware) on the bench.
# ---------------------------------------------------------------------------


def _require_ground(*env_vars: str) -> None:
    missing = [v for v in env_vars if not os.environ.get(v)]
    if missing:
        pytest.skip("two-board ground radio not configured: set " + ", ".join(missing))


def _set_ground_rx_profile(profile: str) -> None:
    cmd = GROUND_CMD.format(profile=PROFILE_IDS[profile])
    subprocess.run(cmd, shell=True, check=True, timeout=60)


@pytest.mark.two_board_rf
def test_06_two_board_pairing_downlink(fprime_test_api: IntegrationTestAPI, start_gds):
    """Flight→ground frame delivery at each profile pairing: for each profile,
    set the ground radio RX profile and the flight TX profile to match, enable
    transmit, and assert RF frames reach the ground radio (raw bytes on its
    data CDC).  Requires USP_GROUND_CMD and USP_GROUND_DATA_TTY."""
    _require_ground("USP_GROUND_CMD", "USP_GROUND_DATA_TTY")
    import serial

    for profile in PROFILE_SWEEP:
        _set_ground_rx_profile(profile)
        _switch_profile(fprime_test_api, "TX", profile)
        proves_send_and_assert_command(
            fprime_test_api, f"{radio}.TRANSMIT", ["ENABLED"]
        )
        try:
            with serial.Serial(GROUND_DATA_TTY, baudrate=115200, timeout=1.0) as ser:
                ser.reset_input_buffer()
                deadline = time.monotonic() + GROUND_READ_WINDOW_S
                rx = bytearray()
                while time.monotonic() < deadline and len(rx) == 0:
                    chunk = ser.read(256)
                    if chunk:
                        rx.extend(chunk)
        finally:
            proves_send_and_assert_command(
                fprime_test_api, f"{radio}.TRANSMIT", ["DISABLED"]
            )
        assert len(rx) > 0, (
            f"No RF bytes reached the ground radio at profile pairing {profile}"
        )


@pytest.mark.two_board_rf
def test_07_two_board_pairing_uplink(fprime_test_api: IntegrationTestAPI, start_gds):
    """Ground→flight frame delivery at each profile pairing: for each profile,
    match the flight RX profile to the ground TX profile, trigger a ground
    transmission (USP_GROUND_UPLINK_CMD), and assert the flight radio saw the
    frame (uspRadio.LastRssi update — set on every received frame)."""
    _require_ground("USP_GROUND_UPLINK_CMD")

    for profile in PROFILE_SWEEP:
        _switch_profile(fprime_test_api, "RX", profile)
        fprime_test_api.clear_histories()
        cmd = GROUND_UPLINK_CMD.format(profile=PROFILE_IDS[profile])
        subprocess.run(cmd, shell=True, check=True, timeout=120)
        result = fprime_test_api.await_telemetry(f"{radio}.LastRssi", timeout=30)
        assert result is not None, (
            f"Flight radio saw no RF frame (no LastRssi update) at profile "
            f"pairing {profile}"
        )

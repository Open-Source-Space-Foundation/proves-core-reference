"""
rf_profile_matrix_test.py:

Integration tests for the USP radio link-profile commands: SET_TX_PROFILE,
SET_RX_PROFILE, CONTINUOUS_WAVE, and the first TX after a profile switch.

Test groups:
  - TX and RX profile sweeps through every LinkProfileId and back to P0.
  - CONTINUOUS_WAVE with restore to RX.
  - Profile switch as the first command after an idle window.
  - Profile switch after a long idle window.
  - GFSK/GMSK first TX after a profile switch.
  - Two-board profile pairing against a ground radio (marked two_board_rf;
    skipped unless the USP_GROUND_* env vars below are set).

Env vars:
  RF_PROFILE_HAMMER_CYCLES  post-wake switch cycles (default 5)
  RF_WEDGE_KILL_CYCLES      first-TX-after-switch repetitions per profile
                            (default 2)
  RF_WEDGE_IDLE_S           idle seconds before the first-TX-after-switch
                            profile switch (default 1)
  RF_PROFILE_WAKE_IDLE_S    idle seconds before each post-wake switch (default 3)
  RF_PROFILE_LONG_IDLE_S    idle seconds for the post-idle test (default 90)
  USP_GROUND_DATA_TTY       ground radio data-CDC device; downlinked RF frames
                            appear here as raw bytes
  USP_GROUND_CMD            shell command template that sets the ground radio RX
                            profile; "{profile}" is replaced with the numeric
                            LinkProfileId
  USP_GROUND_UPLINK_CMD     shell command template that makes the ground radio
                            transmit at least one RF frame; "{profile}" is
                            replaced with the numeric LinkProfileId
"""

import os
import subprocess
import time
from datetime import datetime

import pytest
from common import cmdDispatch, proves_send_and_assert_command
from fprime_gds.common.models.serialize.time_type import TimeType
from fprime_gds.common.testing_fw.api import IntegrationTestAPI

# A profile switch drops any in-flight RF link, so this module only runs when
# the GDS is connected over UART.
pytestmark = [pytest.mark.uart_only]

downlinkDelay = "ReferenceDeployment.downlinkDelay"
radio = "ReferenceDeployment.uspRadio"
tlmSend = "CdhCore.tlmSend"

# TlmPacketizer packet id (Health). SEND_PKT with this id forces a downlink
# frame, and so a radio TX while TRANSMIT is ENABLED.
HEALTH_PACKET_ID = 2

# Events that indicate the radio rejected or failed a reconfiguration.
PROFILE_ERROR_EVENTS = ("ConfigurationFailed", "InvalidProfile")
RADIO_ERROR_EVENTS = ("SendFailed", "ConfigurationFailed", "AllocationFailed")

# LinkProfileId sweep order. P0 is the boot default, so the sweep ends by
# restoring it.
PROFILE_SWEEP = [
    "P1_LORA_SF10",
    "P2_LORA_SF5",
    "P3_GFSK_38K",
    "P4_GFSK_75K",
    "P5_GMSK_83K",
    "P0_LORA_SF8",
]
BOOT_PROFILE = "P0_LORA_SF8"

# Numeric LinkProfileId values for the ground-side command template.
PROFILE_IDS = {
    "P0_LORA_SF8": 0,
    "P1_LORA_SF10": 1,
    "P2_LORA_SF5": 2,
    "P3_GFSK_38K": 3,
    "P4_GFSK_75K": 4,
    "P5_GMSK_83K": 5,
}

# Continuous-wave burst duration (seconds). Kept short so the command completes
# within the GDS command-completion timeout.
CW_SECONDS = 5

# RX auto-revert is disabled (revert_s=0) for all single-board switches. The
# tests restore P0 explicitly; an auto-revert mid-test would race the
# ProfileChanged assertions.
NO_REVERT = 0

HAMMER_CYCLES = int(os.environ.get("RF_PROFILE_HAMMER_CYCLES", "5"))
WEDGE_KILL_CYCLES = int(os.environ.get("RF_WEDGE_KILL_CYCLES", "2"))
WEDGE_IDLE_S = float(os.environ.get("RF_WEDGE_IDLE_S", "1"))
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
    """Send SET_TX_PROFILE or SET_RX_PROFILE and assert the deferred apply
    completed: ProfileChanged is emitted and no profile error events follow."""
    start = _now_start()
    if direction == "TX":
        proves_send_and_assert_command(
            fprime_test_api, f"{radio}.SET_TX_PROFILE", [profile]
        )
    else:
        proves_send_and_assert_command(
            fprime_test_api, f"{radio}.SET_RX_PROFILE", [profile, NO_REVERT]
        )
    # The command handler defers the apply to the component thread. The switch
    # is complete only once ProfileChanged is emitted.
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
    """Set the downlink divider before each test. After each test, disable
    transmit and restore both profiles to the boot default."""
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
    """Sweep SET_TX_PROFILE through every profile in PROFILE_SWEEP, ending at P0.
    Assert each switch emits ProfileChanged with no error events and the board
    stays commandable."""
    for profile in PROFILE_SWEEP:
        _switch_profile(fprime_test_api, "TX", profile)
        # The command path is still alive after the switch.
        proves_send_and_assert_command(fprime_test_api, f"{cmdDispatch}.CMD_NO_OP")


def test_02_rx_profile_sweep(fprime_test_api: IntegrationTestAPI, start_gds):
    """Sweep SET_RX_PROFILE through every profile in PROFILE_SWEEP, ending at P0.
    Assert each switch emits ProfileChanged with no error events and the board
    stays commandable."""
    for profile in PROFILE_SWEEP:
        _switch_profile(fprime_test_api, "RX", profile)
        proves_send_and_assert_command(fprime_test_api, f"{cmdDispatch}.CMD_NO_OP")


def test_03_continuous_wave_restore_to_rx(
    fprime_test_api: IntegrationTestAPI, start_gds
):
    """Run CONTINUOUS_WAVE, then assert the receiver can still be reconfigured
    (RX profile switch away from P0 and back) and that TRANSMIT ENABLED
    produces no radio error events."""
    proves_send_and_assert_command(
        fprime_test_api,
        f"{radio}.CONTINUOUS_WAVE",
        [CW_SECONDS],
    )
    # Wait out the CW duration so the asynchronous restore to RX completes.
    time.sleep(CW_SECONDS + 2)

    # The receiver must be reconfigurable after CW.
    _switch_profile(fprime_test_api, "RX", "P2_LORA_SF5")
    _switch_profile(fprime_test_api, "RX", BOOT_PROFILE)

    # The TX path must also be intact: enabling transmit must produce no radio error events.
    start = _now_start()
    proves_send_and_assert_command(fprime_test_api, f"{radio}.TRANSMIT", ["ENABLED"])
    time.sleep(10)
    for evt in RADIO_ERROR_EVENTS:
        result = fprime_test_api.await_event(f"{radio}.{evt}", start=start, timeout=0)
        assert result is None, f"Unexpected {radio}.{evt} after CW restore: {result}"


def test_04_post_wake_profile_switch_hammer(
    fprime_test_api: IntegrationTestAPI, start_gds
):
    """Idle for RF_PROFILE_WAKE_IDLE_S, then send an RX profile switch as the
    first command; repeat RF_PROFILE_HAMMER_CYCLES times alternating P2 and P0.
    Assert every switch completes with ProfileChanged and no error events."""
    for cycle in range(HAMMER_CYCLES):
        # No commands during the idle window, so the switch below is the first SPI command sequence after the modem sleeps.
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
    """With transmit disabled, idle for RF_PROFILE_LONG_IDLE_S, then send an RX
    profile switch as the first command. Assert the switch completes and the
    board is commandable afterwards."""
    proves_send_and_assert_command(fprime_test_api, f"{radio}.TRANSMIT", ["DISABLED"])
    time.sleep(LONG_IDLE_S)
    _switch_profile(fprime_test_api, "RX", "P1_LORA_SF10")
    _switch_profile(fprime_test_api, "RX", BOOT_PROFILE)
    proves_send_and_assert_command(fprime_test_api, f"{cmdDispatch}.CMD_NO_OP")


def _force_tx_and_await_advance(
    fprime_test_api: IntegrationTestAPI,
    floor: int | None,
    context: str,
    timeout: float = 45.0,
) -> int:
    """Send SEND_PKT to force a downlink frame, wait for uspRadio.BytesSent to
    pass ``floor``, and return the new value. BytesSent increments only when a
    radio transmission completes, so an increase proves the TX went out."""
    proves_send_and_assert_command(
        fprime_test_api, f"{tlmSend}.SEND_PKT", [HEALTH_PACKET_ID, "REALTIME"]
    )
    deadline = time.monotonic() + timeout
    last_seen = floor
    while time.monotonic() < deadline:
        result = fprime_test_api.await_telemetry(f"{radio}.BytesSent", timeout=5)
        if result is not None:
            val = int(result.get_val())
            last_seen = val
            if floor is None or val > floor:
                return val
    raise AssertionError(
        f"uspRadio.BytesSent did not advance past {floor} within {timeout}s "
        f"{context} (last seen: {last_seen})"
    )


@pytest.mark.parametrize("target_profile", ["P4_GFSK_75K", "P5_GMSK_83K"])
def test_08_gfsk_wedge_kill_recipe(
    fprime_test_api: IntegrationTestAPI, start_gds, target_profile
):
    """Force a TX at P0, idle for RF_WEDGE_IDLE_S, switch the TX profile to a
    GFSK/GMSK profile, force a TX immediately, then switch back to P0 and
    force a TX again; repeat RF_WEDGE_KILL_CYCLES times. Assert
    uspRadio.BytesSent advances after every forced TX and no radio error
    events are logged."""
    proves_send_and_assert_command(fprime_test_api, f"{radio}.TRANSMIT", ["ENABLED"])
    try:
        for cycle in range(WEDGE_KILL_CYCLES):
            ctx = f"(cycle {cycle + 1}/{WEDGE_KILL_CYCLES}, {target_profile})"

            # Force a TX at P0 to establish the BytesSent baseline.
            baseline = _force_tx_and_await_advance(
                fprime_test_api, None, f"at P0 baseline {ctx}"
            )

            # Idle before the switch.
            time.sleep(WEDGE_IDLE_S)

            # Switch the TX profile from P0 to the target profile.
            start = _now_start()
            _switch_profile(fprime_test_api, "TX", target_profile)

            # Force the first TX after the switch.
            baseline = _force_tx_and_await_advance(
                fprime_test_api, baseline, f"on first TX after P0->{ctx}"
            )
            for evt in RADIO_ERROR_EVENTS:
                result = fprime_test_api.await_event(
                    f"{radio}.{evt}", start=start, timeout=0
                )
                assert result is None, (
                    f"Unexpected {radio}.{evt} after switch to {target_profile} "
                    f"{ctx}: {result}"
                )

            # Return to P0 and force a TX again.
            start = _now_start()
            _switch_profile(fprime_test_api, "TX", BOOT_PROFILE)
            _force_tx_and_await_advance(
                fprime_test_api, baseline, f"after return to P0 {ctx}"
            )
            for evt in RADIO_ERROR_EVENTS:
                result = fprime_test_api.await_event(
                    f"{radio}.{evt}", start=start, timeout=0
                )
                assert result is None, (
                    f"Unexpected {radio}.{evt} after return to P0 {ctx}: {result}"
                )
    finally:
        proves_send_and_assert_command(
            fprime_test_api, f"{radio}.TRANSMIT", ["DISABLED"]
        )


# ---------------------------------------------------------------------------
# Two-board tests: require a ground radio. Skipped unless the USP_GROUND_* env
# vars are set.
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
    """For each profile, set the ground radio RX profile and the flight TX
    profile to match, enable transmit, and assert raw RF bytes arrive on the
    ground radio data CDC. Requires USP_GROUND_CMD and USP_GROUND_DATA_TTY."""
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
    """For each profile, set the flight RX profile to match the ground TX
    profile, trigger a ground transmission with USP_GROUND_UPLINK_CMD, and
    assert uspRadio.LastRssi updates (it is set on every received frame).
    Requires USP_GROUND_UPLINK_CMD."""
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

"""
startup_manager_test.py:

Integration tests for the Startup Manager component.
"""

import tempfile
from pathlib import Path

import pytest
from common import proves_send_and_assert_command
from fprime_gds.common.testing_fw.api import IntegrationTestAPI

pytestmark = [pytest.mark.uart_only]

startupManager = "ReferenceDeployment.startupManager"
resetManager = "ReferenceDeployment.resetManager"

QUIESCENCE_START_FILE = "/quiescence_start.bin"
# Fw::Time serializes to 11 bytes (time base, context, seconds, useconds). All 0xFF is what an erased or torn
# flash write leaves behind; it fails Fw::Time deserialization.
CORRUPT_TIME_BYTES = b"\xff" * 11


def _get_boot_count(fprime_test_api: IntegrationTestAPI) -> int:
    proves_send_and_assert_command(fprime_test_api, f"{startupManager}.GET_BOOT_COUNT")
    return (
        fprime_test_api.assert_event(f"{startupManager}.CurrentBootCount", timeout=3)
        .args[0]
        .val
    )


def _cold_reset_and_await_first_tick(
    fprime_test_api: IntegrationTestAPI, boot_count: int
):
    """Reset, then wait for the BootCount channel, which is written at the end of the first run tick."""
    fprime_test_api.clear_histories()
    fprime_test_api.send_command(f"{resetManager}.COLD_RESET")
    fprime_test_api.assert_telemetry(
        f"{startupManager}.BootCount", value=boot_count, timeout=20
    )


def test_01_corrupt_quiescence_file_boots_cleanly(
    fprime_test_api: IntegrationTestAPI, start_gds
):
    """A corrupt quiescence start file is replaced on boot instead of asserting (#547, REQ-SM-012).

    Before #547 this file made every boot assert in StartupManager and the board watchdog-looped.
    The test uplinks erased-flash bytes over the file, resets, and checks that the board reports
    QuiescenceFileUnreadable, boots exactly once, and that the next boot reads the rewritten file
    without reporting it again.

    Side effect: the quiescence start time restarts at the time of the test, as on a freshly
    formatted board.
    """
    with tempfile.TemporaryDirectory() as tempdir:
        corrupt_file = Path(tempdir) / "quiescence_start.bin"
        corrupt_file.write_bytes(CORRUPT_TIME_BYTES)
        fprime_test_api.uplink_file(corrupt_file, QUIESCENCE_START_FILE)
        fprime_test_api.assert_event("FileReceived", timeout=10)

    initial_boot_count = _get_boot_count(fprime_test_api)

    # Exactly one boot: a boot loop would bump the count more than once or stop answering.
    _cold_reset_and_await_first_tick(fprime_test_api, initial_boot_count + 1)
    fprime_test_api.assert_event_count(
        1, f"{startupManager}.QuiescenceFileUnreadable", timeout=5
    )

    # The second boot reads the rewritten file without reporting it.
    _cold_reset_and_await_first_tick(fprime_test_api, initial_boot_count + 2)
    fprime_test_api.assert_event_count(0, f"{startupManager}.QuiescenceFileUnreadable")

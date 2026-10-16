"""
startup_manager_test.py:

Integration tests for the Startup Manager component.
"""

import tempfile
import time
from datetime import datetime
from pathlib import Path

import pytest
from common import proves_send_and_assert_command
from fprime_gds.common.models.serialize.time_type import TimeType
from fprime_gds.common.testing_fw.api import IntegrationTestAPI

pytestmark = [pytest.mark.uart_only]

startupManager = "ReferenceDeployment.startupManager"
resetManager = "ReferenceDeployment.resetManager"

QUIESCENCE_START_FILE = "/quiescence_start.bin"
# Fw::Time serializes to 11 bytes (time base, context, seconds, useconds). All 0xFF is what an erased or torn
# flash write leaves behind; it fails Fw::Time deserialization.
CORRUPT_TIME_BYTES = b"\xff" * 11


def _now() -> TimeType:
    return TimeType().set_datetime(
        datetime.now(), time_base=TimeType.TimeBase("TB_DONT_CARE")
    )


def _get_boot_count(fprime_test_api: IntegrationTestAPI) -> int:
    fprime_test_api.clear_histories()
    proves_send_and_assert_command(fprime_test_api, f"{startupManager}.GET_BOOT_COUNT")
    return (
        fprime_test_api.assert_event(f"{startupManager}.CurrentBootCount", timeout=3)
        .args[0]
        .val
    )


def _cold_reset_and_await_boot(fprime_test_api: IntegrationTestAPI) -> TimeType:
    start = _now()
    fprime_test_api.send_command(f"{resetManager}.COLD_RESET")
    fprime_test_api.assert_event(
        "CdhCore.version.FrameworkVersion", start=start, timeout=15
    )
    return start


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

    start = _cold_reset_and_await_boot(fprime_test_api)
    fprime_test_api.assert_event(
        f"{startupManager}.QuiescenceFileUnreadable", start=start, timeout=10
    )
    # Exactly one boot: a boot loop would bump the count more than once or stop answering.
    assert _get_boot_count(fprime_test_api) == initial_boot_count + 1

    # The second boot reads the rewritten file on its first tick. Check for the event before
    # _get_boot_count, which clears the event history; the boot count then proves that tick ran.
    start = _cold_reset_and_await_boot(fprime_test_api)
    time.sleep(5)
    fprime_test_api.assert_event_count(
        0, f"{startupManager}.QuiescenceFileUnreadable", start=start
    )
    assert _get_boot_count(fprime_test_api) == initial_boot_count + 2

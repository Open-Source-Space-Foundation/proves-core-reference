"""
mosaic_manager_test.py:

Integration tests for the MosaicManager component.

The MOSAIC gamma ray payload streams "ADC=<raw>,MV=<millivolts>" CSV lines
over UART. MosaicManager parses the lines and stores the samples on disk
under /mosaic as raw binary records for later downlink.

Marked uart_only: the assertions below chase multi-step event sequences
(FLUSH -> SampleFileClosed -> FileSize), which the half-duplex LoRa link in
the radio job cannot carry reliably within the per-test timeouts.
"""

import time
from datetime import datetime

import pytest
from common import proves_send_and_assert_command
from fprime_gds.common.models.serialize.time_type import TimeType
from fprime_gds.common.testing_fw.api import IntegrationTestAPI

pytestmark = [pytest.mark.uart_only]

mosaicManager = "ReferenceDeployment.mosaicManager"
fileManager = "FileHandling.fileManager"
modeManager = "ReferenceDeployment.modeManager"

# Time for the mode manager to leave SAFE_MODE and release the load switches
SAFE_MODE_EXIT_SETTLE_SECONDS = 5

# MosaicManager serializes each sample as U32 seconds + U16 ADC + U16 millivolts
RECORD_SIZE = 8

# MOSAIC emits a sample every 100 ms; 5 s is comfortably enough to see several
# arrive without making the suite slow.
SAMPLE_ACCUMULATION_SECONDS = 5

# Telemetry packets are gated by telemetryDelay (Utilities.RateDelay on the 1 Hz
# rate group), which releases CdhCore.tlmSend.Run every 30 s -- the packetizer's
# ON_CHANGE_MIN/min=0/max=0 group config does not make packets any more frequent
# than that. proves_send_and_assert_command clears the histories before sending,
# so a telemetry assertion after a command always waits for a fresh packet.
# Allow two and a half downlink periods so a single missed window is not a
# failure. Events are not gated this way, which is why they use short timeouts.
TLM_DOWNLINK_PERIOD_SECONDS = 30
TLM_TIMEOUT_SECONDS = 75

# Use a larger per-file limit during the tests so the 10 Hz payload cannot
# automatically close the current file just before an explicit FLUSH. The CI
# filesystem is formatted before and after the suite, so the flight file-count
# limit can remain unchanged.
TEST_PARAMETERS = {
    "SAMPLES_PER_FILE": 1000,
    "SAMPLES_PER_WRITE": 10,
    "MAX_FILE_COUNT": 40,
    "MAX_FILESYSTEM_ERRORS": 5,
}

DEFAULT_PARAMETERS = {
    "SAMPLES_PER_FILE": 100,
    "SAMPLES_PER_WRITE": 10,
    "MAX_FILE_COUNT": 40,
    "MAX_FILESYSTEM_ERRORS": 5,
}


def _now() -> TimeType:
    return TimeType().set_datetime(
        datetime.now(), time_base=TimeType.TimeBase("TB_DONT_CARE")
    )


def _exit_safe_mode_if_needed(fprime_test_api: IntegrationTestAPI) -> None:
    """Bring the FSW out of SAFE_MODE so payload power can be commanded on.

    mode_manager_test.py sorts immediately before this file and its
    test_safe_09 case deliberately drives the FSW into SAFE_MODE and reboots.
    Its own teardown normally recovers, but when that test fails the recovery
    does not complete, and conftest's recover_from_safe_mode fixture only runs
    on the radio pass. In SAFE_MODE the mode manager holds the payload load
    switch off, so MOSAIC is unpowered and every assertion here fails for a
    reason that has nothing to do with MosaicManager. Recover explicitly rather
    than inheriting whatever the previous file left behind.
    """
    try:
        fprime_test_api.clear_histories()
        fprime_test_api.send_and_assert_command(
            f"{modeManager}.GET_CURRENT_MODE", timeout=10, max_delay=10
        )
        evt = fprime_test_api.await_event(
            f"{modeManager}.CurrentModeReading", timeout=5
        )
        if evt is not None and "SAFE_MODE" in str(evt.args[0].val).upper():
            fprime_test_api.send_command(f"{modeManager}.EXIT_SAFE_MODE")
            time.sleep(SAFE_MODE_EXIT_SETTLE_SECONDS)
    except AssertionError:
        # The board may still be rebooting out of test_safe_09; powering the
        # payload below will fail loudly enough on its own.
        pass


@pytest.fixture(autouse=True)
def setup_test(fprime_test_api: IntegrationTestAPI, start_gds):
    """Power and configure MOSAIC, then restore flight defaults after each test."""
    _exit_safe_mode_if_needed(fprime_test_api)

    # Stop first so configuration changes cannot race an open sample file left
    # behind by an earlier test.
    proves_send_and_assert_command(
        fprime_test_api,
        f"{mosaicManager}.STOP_RECORDING",
    )

    for param, value in TEST_PARAMETERS.items():
        proves_send_and_assert_command(
            fprime_test_api, f"{mosaicManager}.{param}_PRM_SET", [value]
        )

    proves_send_and_assert_command(
        fprime_test_api,
        "ReferenceDeployment.payloadPowerLoadSwitch.TURN_ON",
    )
    proves_send_and_assert_command(
        fprime_test_api,
        f"{mosaicManager}.START_RECORDING",
    )
    time.sleep(SAMPLE_ACCUMULATION_SECONDS)  # Payload powers on and starts streaming

    yield

    proves_send_and_assert_command(
        fprime_test_api,
        f"{mosaicManager}.STOP_RECORDING",
    )
    for param, value in DEFAULT_PARAMETERS.items():
        proves_send_and_assert_command(
            fprime_test_api, f"{mosaicManager}.{param}_PRM_SET", [value]
        )
    fprime_test_api.clear_histories()


def test_01_start_stop_recording(fprime_test_api: IntegrationTestAPI, start_gds):
    """Test that recording can be stopped and started"""
    start = _now()

    proves_send_and_assert_command(
        fprime_test_api,
        f"{mosaicManager}.STOP_RECORDING",
    )
    fprime_test_api.assert_event(
        f"{mosaicManager}.RecordingStopped", start=start, timeout=10
    )
    # Match on the value, and do not pass start=: the 1 Hz rate group also
    # publishes Recording, so a sample emitted between the command being sent
    # and the handler running still carries the old value, and a start= built
    # from the host's datetime.now() is compared against board timestamps --
    # any clock skew silently excludes every sample that follows.
    # proves_send_and_assert_command clears the histories immediately before
    # sending, so the history already starts at the command.
    fprime_test_api.assert_telemetry(
        f"{mosaicManager}.Recording", value=False, timeout=TLM_TIMEOUT_SECONDS
    )

    start = _now()
    proves_send_and_assert_command(
        fprime_test_api,
        f"{mosaicManager}.START_RECORDING",
    )
    fprime_test_api.assert_event(
        f"{mosaicManager}.RecordingStarted", start=start, timeout=10
    )
    fprime_test_api.assert_telemetry(
        f"{mosaicManager}.Recording", value=True, timeout=TLM_TIMEOUT_SECONDS
    )
    fprime_test_api.assert_telemetry(
        f"{mosaicManager}.FilesystemErrors",
        value=0,
        timeout=TLM_TIMEOUT_SECONDS,
    )


def test_02_samples_recorded(fprime_test_api: IntegrationTestAPI, start_gds):
    """Test that samples stream in from the payload and are recorded"""
    time.sleep(SAMPLE_ACCUMULATION_SECONDS)

    result = fprime_test_api.assert_telemetry(
        f"{mosaicManager}.SamplesRecorded", timeout=TLM_TIMEOUT_SECONDS
    )
    assert result.get_val() > 0, (
        "MosaicManager recorded no samples; is the MOSAIC payload attached to "
        "the peripheral UART and powered?"
    )

    # A parsed sample must also surface the raw reading, proving the ASCII
    # protocol was decoded rather than just bytes being counted.
    fprime_test_api.assert_telemetry(
        f"{mosaicManager}.LatestAdc", timeout=TLM_TIMEOUT_SECONDS
    )
    fprime_test_api.assert_telemetry(
        f"{mosaicManager}.LatestMillivolts", timeout=TLM_TIMEOUT_SECONDS
    )


def test_03_flush_writes_file_to_filesystem(
    fprime_test_api: IntegrationTestAPI, start_gds
):
    """Test that FLUSH closes the sample file and it exists on disk under /mosaic"""
    time.sleep(SAMPLE_ACCUMULATION_SECONDS)

    start = _now()
    proves_send_and_assert_command(
        fprime_test_api,
        f"{mosaicManager}.FLUSH",
    )
    closed = fprime_test_api.assert_event(
        f"{mosaicManager}.SampleFileClosed", start=start, timeout=10
    )

    assert len(closed.get_args()) == 2
    file_name = closed.args[0].val
    records = closed.args[1].val
    assert file_name.startswith("/mosaic/"), (
        f"MOSAIC samples must be stored under /mosaic, got {file_name}"
    )
    assert records > 0, "FLUSH closed a file containing no samples"

    # Ask the flight software's own file manager for the size on disk; this is
    # the end-to-end proof that the bytes actually reached the filesystem and
    # are available for downlink, not just that the component thinks they did.
    start = _now()
    proves_send_and_assert_command(
        fprime_test_api,
        f"{fileManager}.FileSize",
        [file_name],
    )
    sized = fprime_test_api.assert_event(
        f"{fileManager}.FileSizeSucceeded", start=start, timeout=15
    )
    assert len(sized.get_args()) == 2
    size = sized.args[1].val
    assert size == records * RECORD_SIZE, (
        f"{file_name} holds {size} B on disk but {records} samples were "
        f"reported ({records * RECORD_SIZE} B expected)"
    )


def test_04_max_file_count_stops_recording(
    fprime_test_api: IntegrationTestAPI, start_gds
):
    """Test that reaching MAX_FILE_COUNT emits an event and stops recording."""
    proves_send_and_assert_command(
        fprime_test_api,
        f"{mosaicManager}.STOP_RECORDING",
    )

    try:
        proves_send_and_assert_command(
            fprime_test_api,
            f"{mosaicManager}.MAX_FILE_COUNT_PRM_SET",
            [0],
        )
        proves_send_and_assert_command(
            fprime_test_api,
            f"{mosaicManager}.START_RECORDING",
        )

        reached = fprime_test_api.assert_event(
            f"{mosaicManager}.MaxFilesReached", timeout=10
        )
        assert reached.args[0].val == 0
        fprime_test_api.assert_telemetry(
            f"{mosaicManager}.Recording",
            value=False,
            timeout=TLM_TIMEOUT_SECONDS,
        )
        fprime_test_api.assert_telemetry(
            f"{mosaicManager}.FilesystemErrors",
            value=0,
            timeout=TLM_TIMEOUT_SECONDS,
        )
    finally:
        proves_send_and_assert_command(
            fprime_test_api,
            f"{mosaicManager}.MAX_FILE_COUNT_PRM_SET",
            [TEST_PARAMETERS["MAX_FILE_COUNT"]],
        )
        proves_send_and_assert_command(
            fprime_test_api,
            f"{mosaicManager}.START_RECORDING",
        )

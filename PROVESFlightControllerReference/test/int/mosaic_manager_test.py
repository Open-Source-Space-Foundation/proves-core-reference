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

# MosaicManager serializes each sample as U32 seconds + U16 ADC + U16 millivolts
RECORD_SIZE = 8

# MOSAIC emits a sample every 100 ms; 5 s is comfortably enough to see several
# arrive without making the suite slow.
SAMPLE_ACCUMULATION_SECONDS = 5


def _now() -> TimeType:
    return TimeType().set_datetime(
        datetime.now(), time_base=TimeType.TimeBase("TB_DONT_CARE")
    )


@pytest.fixture(autouse=True)
def setup_test(fprime_test_api: IntegrationTestAPI, start_gds):
    """Fixture to power the MOSAIC payload before each test"""
    proves_send_and_assert_command(
        fprime_test_api,
        "ReferenceDeployment.payloadPowerLoadSwitch.TURN_ON",
    )
    time.sleep(SAMPLE_ACCUMULATION_SECONDS)  # Payload powers on and starts streaming
    # Every test below assumes recording is active; a previous test that failed
    # midway through test_01 could otherwise leave it stopped.
    proves_send_and_assert_command(
        fprime_test_api,
        f"{mosaicManager}.START_RECORDING",
    )


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
    # Match on the value rather than taking the first sample in the window: the
    # 1 Hz rate group also emits Recording, so a sample published between the
    # command being sent and the handler running still carries the old value.
    fprime_test_api.assert_telemetry(
        f"{mosaicManager}.Recording", value=False, start=start, timeout=15
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
        f"{mosaicManager}.Recording", value=True, start=start, timeout=15
    )


def test_02_samples_recorded(fprime_test_api: IntegrationTestAPI, start_gds):
    """Test that samples stream in from the payload and are recorded"""
    time.sleep(SAMPLE_ACCUMULATION_SECONDS)

    result = fprime_test_api.assert_telemetry(
        f"{mosaicManager}.SamplesRecorded", timeout=10
    )
    assert result.get_val() > 0, (
        "MosaicManager recorded no samples; is the MOSAIC payload attached to "
        "the peripheral UART and powered?"
    )

    # A parsed sample must also surface the raw reading, proving the ASCII
    # protocol was decoded rather than just bytes being counted.
    fprime_test_api.assert_telemetry(f"{mosaicManager}.LatestAdc", timeout=10)
    fprime_test_api.assert_telemetry(f"{mosaicManager}.LatestMillivolts", timeout=10)


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

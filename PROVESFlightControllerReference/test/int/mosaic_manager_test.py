"""
mosaic_manager_test.py:

Integration tests for the MosaicManager component.

The MOSAIC gamma ray payload streams "ADC=<raw>,MV=<millivolts>" CSV lines
over UART. MosaicManager parses the lines and stores the samples on disk as
F Prime data products.
"""

import os
import time
from datetime import datetime

import pytest
from common import proves_send_and_assert_command
from fprime_gds.common.models.serialize.time_type import TimeType
from fprime_gds.common.testing_fw.api import IntegrationTestAPI

mosaicManager = "ReferenceDeployment.mosaicManager"


@pytest.fixture(autouse=True)
def setup_test(fprime_test_api: IntegrationTestAPI, start_gds):
    """Fixture to power the MOSAIC payload before each test"""
    proves_send_and_assert_command(
        fprime_test_api,
        "ReferenceDeployment.payloadPowerLoadSwitch.TURN_ON",
    )
    time.sleep(5)  # Wait for the payload to power on and start streaming


def test_01_start_stop_recording(fprime_test_api: IntegrationTestAPI, start_gds):
    """Test that recording can be stopped and started"""
    start: TimeType = TimeType().set_datetime(
        datetime.now(), time_base=TimeType.TimeBase("TB_DONT_CARE")
    )

    proves_send_and_assert_command(
        fprime_test_api,
        f"{mosaicManager}.STOP_RECORDING",
    )
    fprime_test_api.assert_event(
        f"{mosaicManager}.RecordingStopped", start=start, timeout=10
    )

    proves_send_and_assert_command(
        fprime_test_api,
        f"{mosaicManager}.START_RECORDING",
    )
    fprime_test_api.assert_event(
        f"{mosaicManager}.RecordingStarted", start=start, timeout=10
    )


requires_mosaic = pytest.mark.skipif(
    not os.environ.get("MOSAIC_ATTACHED"),
    reason="Requires MOSAIC payload attached (set MOSAIC_ATTACHED=1)",
)


@requires_mosaic
def test_02_samples_recorded(fprime_test_api: IntegrationTestAPI, start_gds):
    """Test that samples stream in and are recorded"""
    # MOSAIC sends a sample every 100 ms; wait for a few to arrive
    time.sleep(5)

    result = fprime_test_api.assert_telemetry(
        f"{mosaicManager}.SamplesRecorded", timeout=10
    )
    assert result.get_val() > 0


@requires_mosaic
def test_03_flush_writes_data_product(fprime_test_api: IntegrationTestAPI, start_gds):
    """Test that FLUSH sends a partially filled data product to disk"""
    start: TimeType = TimeType().set_datetime(
        datetime.now(), time_base=TimeType.TimeBase("TB_DONT_CARE")
    )

    # Let some samples accumulate, then flush the open container
    time.sleep(5)
    proves_send_and_assert_command(
        fprime_test_api,
        f"{mosaicManager}.FLUSH",
    )
    fprime_test_api.assert_event(
        f"{mosaicManager}.DataProductSent", start=start, timeout=10
    )

    # DpWriter reports the file write on the ground path as well
    fprime_test_api.assert_event(
        "ReferenceDeployment.DataProducts.dpWriter.FileWritten", start=start, timeout=10
    )

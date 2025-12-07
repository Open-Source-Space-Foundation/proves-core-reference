"""
tmp112_test.py:

Integration tests for the TMP112 Manager component.
"""

from datetime import datetime

import pytest
from common import proves_send_and_assert_command
from fprime.common.models.serialize.time_type import TimeType
from fprime_gds.common.testing_fw.api import IntegrationTestAPI

cameraHandler = "ReferenceDeployment.cameraHandler"


@pytest.fixture(autouse=True)
def setup_test(fprime_test_api: IntegrationTestAPI, start_gds):
    """Fixture to turn on camera payload before each test"""
    proves_send_and_assert_command(
        fprime_test_api,
        "ReferenceDeployment.payloadPowerLoadSwitch.TURN_ON",
        [],
    )


def test_01_get_temperature(fprime_test_api: IntegrationTestAPI, start_gds):
    """Test that we can get temperature"""
    start: TimeType = TimeType().set_datetime(datetime.now())

    # Send command to get temperature
    proves_send_and_assert_command(
        fprime_test_api,
        f"{cameraHandler}.TAKE_IMAGE",
    )
    fprime_test_api.assert_event(
        f"{cameraHandler}.ChunkWritten", start=start, timeout=2
    )

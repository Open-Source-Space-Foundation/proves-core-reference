"""
tmp112_test.py:

Integration tests for the TMP112 Manager component.
"""

import time
from datetime import datetime

import pytest
from common import proves_send_and_assert_command
from fprime_gds.common.models.serialize.time_type import TimeType
from fprime_gds.common.testing_fw.api import IntegrationTestAPI

cameraHandler = "ReferenceDeployment.cameraHandler"


@pytest.fixture(autouse=True)
def setup_test(fprime_test_api: IntegrationTestAPI, start_gds):
    """Fixture to turn on face 4 before each test"""
    proves_send_and_assert_command(
        fprime_test_api,
        "ReferenceDeployment.face4LoadSwitch.TURN_ON",
    )
    time.sleep(5)  # Wait for the camera to power on


def test_01_take_image(fprime_test_api: IntegrationTestAPI, start_gds):
    """Test that we can take an image"""
    start: TimeType = TimeType().set_datetime(
        datetime.now(), time_base=TimeType.TimeBase("TB_DONT_CARE")
    )

    # Send command to take an image
    proves_send_and_assert_command(
        fprime_test_api,
        f"{cameraHandler}.TAKE_IMAGE",
    )
    fprime_test_api.assert_event(
        f"{cameraHandler}.ImageTransferStarted", start=start, timeout=10
    )

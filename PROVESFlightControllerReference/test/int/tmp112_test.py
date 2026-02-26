"""
tmp112_test.py:

Integration tests for the TMP112 Manager component.
"""

from datetime import datetime

import pytest
from common import proves_send_and_assert_command
from fprime_gds.common.data_types.event_data import EventData
from fprime_gds.common.models.serialize.numerical_types import F32Type
from fprime_gds.common.models.serialize.time_type import TimeType
from fprime_gds.common.testing_fw.api import IntegrationTestAPI

tmp112Face0Manager = "ReferenceDeployment.tmp112Face0Manager"


@pytest.fixture(autouse=True)
def setup_test(fprime_test_api: IntegrationTestAPI, start_gds):
    """Fixture to turn on face 0 before each test"""
    proves_send_and_assert_command(
        fprime_test_api,
        "ReferenceDeployment.face0LoadSwitch.TURN_ON",
        [],
    )


def test_01_get_temperature(fprime_test_api: IntegrationTestAPI, start_gds):
    """Test that we can get temperature"""
    start: TimeType = TimeType().set_datetime(
        datetime.now(), time_base=TimeType.TimeBase("TB_DONT_CARE")
    )

    # Send command to get temperature
    proves_send_and_assert_command(
        fprime_test_api,
        f"{tmp112Face0Manager}.GetTemperature",
    )
    result: EventData = fprime_test_api.assert_event(
        f"{tmp112Face0Manager}.Temperature", start=start, timeout=2
    )

    assert result is not None
    assert len(result.get_args()) == 1
    temperature: F32Type = result.args[0]
    assert temperature.val >= 0.0

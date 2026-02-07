"""
veml6031_test.py:

Integration tests for the VEML6031 Manager component.
"""

from datetime import datetime

import pytest
from common import proves_send_and_assert_command
from fprime.common.models.serialize.numerical_types import F32Type
from fprime_gds.common.data_types.event_data import EventData
from fprime_gds.common.models.serialize.time_type import TimeType
from fprime_gds.common.testing_fw.api import IntegrationTestAPI

veml6031Face0Manager = "ReferenceDeployment.veml6031Face0Manager"


@pytest.fixture(autouse=True)
def setup_test(fprime_test_api: IntegrationTestAPI, start_gds):
    """Fixture to turn on face 0 before each test"""
    proves_send_and_assert_command(
        fprime_test_api,
        "ReferenceDeployment.face0LoadSwitch.TURN_ON",
        [],
    )


@pytest.mark.flaky
def test_01_get_visible_light(fprime_test_api: IntegrationTestAPI, start_gds):
    """Test that we can get visible light"""
    start: TimeType = TimeType().set_datetime(
        datetime.now(), time_base=TimeType.TimeBase("TB_DONT_CARE")
    )

    # Send command to get visible light
    proves_send_and_assert_command(
        fprime_test_api,
        f"{veml6031Face0Manager}.GetVisibleLight",
    )
    result: EventData = fprime_test_api.assert_event(
        f"{veml6031Face0Manager}.VisibleLight", start=start, timeout=2
    )

    assert result is not None
    assert len(result.get_args()) == 1
    visible_light: F32Type = result.args[0]
    assert visible_light.val >= 0.0

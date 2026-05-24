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
    result: EventData | None = None
    for attempt in range(3):
        start: TimeType = TimeType().set_datetime(
            datetime.now(), time_base=TimeType.TimeBase("TB_DONT_CARE")
        )
        fprime_test_api.clear_histories()
        try:
            # retries=1 so clear_histories() is only called once per outer attempt,
            # preventing a retry inside proves_send_and_assert_command from clearing
            # a Temperature event that arrived between inner retry attempts.
            proves_send_and_assert_command(
                fprime_test_api,
                f"{tmp112Face0Manager}.GetTemperature",
                retries=1,
            )
            result = fprime_test_api.assert_event(
                f"{tmp112Face0Manager}.Temperature", start=start, timeout=5
            )
            break
        except AssertionError:
            if attempt == 2:
                raise

    assert result is not None
    assert len(result.get_args()) == 1
    temperature: F32Type = result.args[0]
    assert temperature.val >= 0.0

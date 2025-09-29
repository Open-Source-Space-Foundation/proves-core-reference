"""
rtc_test.py:

Integration tests for the RTC Manager component.
"""

import json
import time
from datetime import datetime, timezone

import pytest
from fprime.common.models.serialize.numerical_types import U32Type
from fprime.common.models.serialize.time_type import TimeType
from fprime_gds.common.data_types.ch_data import ChData
from fprime_gds.common.data_types.event_data import EventData
from fprime_gds.common.testing_fw.api import IntegrationTestAPI


@pytest.fixture(autouse=True)
def set_now_time(fprime_test_api: IntegrationTestAPI):
    """Fixture to set the time to test runner's time before and after each test"""
    set_time(fprime_test_api)
    fprime_test_api.clear_histories()
    yield
    set_time(fprime_test_api)
    fprime_test_api.clear_histories()


def set_time(fprime_test_api: IntegrationTestAPI, dt: datetime = None):
    """Helper function to set the time to now or to a specified datetime"""
    if dt is None:
        dt = datetime.now(timezone.utc)

    time_data = dict(
        Year=dt.year,
        Month=dt.month,
        Day=dt.day,
        Hour=dt.hour,
        Minute=dt.minute,
        Second=dt.second,
    )
    time_data_str = json.dumps(time_data)
    fprime_test_api.send_and_assert_command(
        "ReferenceDeployment.rtcManager.SET_TIME",
        [
            time_data_str,
        ],
        max_delay=2,
    )
    fprime_test_api.assert_event("ReferenceDeployment.rv3028Manager.TimeSet", timeout=2)


def get_time(fprime_test_api: IntegrationTestAPI) -> datetime:
    """Helper function to request packet and get fresh WatchdogTransitions telemetry"""
    fprime_test_api.clear_histories()
    fprime_test_api.send_and_assert_command(
        "ReferenceDeployment.rtcManager.GET_TIME", max_delay=2
    )
    result: EventData = fprime_test_api.assert_event(
        "ReferenceDeployment.rtcManager.GetTime", timeout=2
    )
    return datetime.fromisoformat(result.display_text)


def test_01_current_time_set(fprime_test_api: IntegrationTestAPI):
    """Test that we can set current time"""

    # Fetch time from fixture setting current time
    event_time = get_time(fprime_test_api)

    # Assert time is within 30 seconds of now
    pytest.approx(event_time, abs=30) == datetime.now(timezone.utc)


def test_02_time_incrementing(fprime_test_api: IntegrationTestAPI):
    """Test that time increments over time"""

    # Fetch initial time
    initial_event_time = get_time(fprime_test_api)

    # Wait for time to increment
    time.sleep(2.0)

    # Fetch updated time
    updated_event_time = get_time(fprime_test_api)

    # Assert time has increased
    assert updated_event_time > initial_event_time, (
        f"Time should increase. Initial: {initial_event_time}, Updated: {updated_event_time}"
    )


def test_03_set_time_in_past(fprime_test_api: IntegrationTestAPI):
    """Test that we can set the time to a past time"""

    # Set time to Curiosity landing on Mars (7 minutes of terror! https://youtu.be/Ki_Af_o9Q9s)
    curiosity_landing = datetime(2012, 8, 6, 5, 17, 57, tzinfo=timezone.utc)
    set_time(fprime_test_api, curiosity_landing)

    # Fetch event data
    result: EventData = fprime_test_api.assert_event(
        "ReferenceDeployment.rv3028Manager.TimeSet", timeout=2
    )

    # Fetch previously set time from event args
    event_previous_time_arg: U32Type = result.args[0]
    previously_set_time = datetime.fromtimestamp(
        event_previous_time_arg.val, tz=timezone.utc
    )

    # Fetch FPrime time from event
    fp_time: TimeType = result.get_time()
    event_time = datetime.fromtimestamp(fp_time.seconds, tz=timezone.utc)

    # Assert previously set time is within 30 seconds of now
    pytest.approx(previously_set_time, abs=30) == datetime.now(timezone.utc)

    # Assert event time is within 30 seconds of curiosity landing
    pytest.approx(event_time, abs=30) == curiosity_landing


def test_04_time_in_telemetry(fprime_test_api: IntegrationTestAPI):
    """Test that we can get Time telemetry"""

    # Fetch telemetry packet
    result: ChData = fprime_test_api.assert_telemetry(
        "CdhCore.cmdDisp.CommandsDispatched", timeout=3
    )

    # Convert FPrime time to datetime
    fp_time: TimeType = result.time
    tlm_time = datetime.fromtimestamp(fp_time.seconds, tz=timezone.utc)

    # Assert time is within 30 seconds of now
    pytest.approx(tlm_time, abs=30) == datetime.now(timezone.utc)

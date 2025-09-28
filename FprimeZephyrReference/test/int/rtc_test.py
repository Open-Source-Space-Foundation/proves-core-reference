"""
watchdog_test.py:

Simple integration tests for the Watchdog component.
Tests are ordered so that stop tests run last.
"""

import time
import pytest
from datetime import datetime, timezone
import json

@pytest.fixture(autouse=True)
def set_now_time(fprime_test_api):
    """Fixture to set the time to test runner's time before and after each test"""
    set_time(fprime_test_api)
    yield
    set_time(fprime_test_api)

def set_time(fprime_test_api, dt: datetime = None):
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
        max_delay=2
    )
    fprime_test_api.assert_event(
        "ReferenceDeployment.rv3028Manager.TimeSet",
        timeout=2
    )

def get_time(fprime_test_api):
    """Helper function to request packet and get fresh WatchdogTransitions telemetry"""
    fprime_test_api.clear_histories()
    fprime_test_api.send_and_assert_command("ReferenceDeployment.rtcManager.GET_TIME", max_delay=2)
    result = fprime_test_api.assert_event(
        "ReferenceDeployment.rtcManager.GetTime",
        timeout=2
    )

    # EventData get_dict() https://github.com/nasa/fprime-gds/blob/67d0ec62f829ed23d72776f1d323f71eaafd31cc/src/fprime_gds/common/data_types/event_data.py#L130C9-L130C17
    return result.get_dict()


def test_01_current_time_set(fprime_test_api):
    """Test that we can read WatchdogTransitions telemetry"""
    event_data = get_time(fprime_test_api)
    event_display_text_time = datetime.fromisoformat(event_data["display_text"])

    # Assert time is within 30 seconds of now
    pytest.approx(event_display_text_time, abs=30) == datetime.now(timezone.utc)


def test_02_set_time_in_past(fprime_test_api):
    """Test that we can set the time to a past time"""
    curiosity_landing = datetime(2012, 8, 6, 5, 17, 57, tzinfo=timezone.utc)
    set_time(fprime_test_api, curiosity_landing)

    event_data = get_time(fprime_test_api)
    event_display_text_time = datetime.fromisoformat(event_data["display_text"])

    # Assert time is within 30 seconds of curiosity landing
    pytest.approx(event_display_text_time, abs=30) == curiosity_landing


def test_03_time_incrementing(fprime_test_api):
    """Test that time increments over time"""
    initial_event_data = get_time(fprime_test_api)
    initial_time = datetime.fromisoformat(initial_event_data["display_text"])

    time.sleep(2.0)  # Wait for time to increment

    updated_event_data = get_time(fprime_test_api)
    updated_time = datetime.fromisoformat(updated_event_data["display_text"])

    assert updated_time > initial_time, \
        f"Time should increase. Initial: {initial_time}, Updated: {updated_time}"


def test_04_time_in_telemetry(fprime_test_api):
    """Test that we can get Time telemetry"""
    # result in ChData object https://github.com/nasa/fprime-gds/blob/67d0ec62f829ed23d72776f1d323f71eaafd31cc/src/fprime_gds/common/data_types/ch_data.py#L18
    result = fprime_test_api.assert_telemetry(
        "CdhCore.cmdDisp.CommandsDispatched",
        timeout=3
    )

    # result.time is TimeType object https://github.com/nasa/fprime-tools/blob/aaa5840181146ca38b195c2a4d3f1bcbb35234c1/src/fprime/common/models/serialize/time_type.py#L49
    tlm_time = datetime.fromtimestamp(result.time.seconds, tz=timezone.utc)

    # Assert time is within 30 seconds of now
    pytest.approx(tlm_time, abs=30) == datetime.now(timezone.utc)

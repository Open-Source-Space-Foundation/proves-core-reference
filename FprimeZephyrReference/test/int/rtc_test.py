"""
rtc_test.py:

Integration tests for the RTC Manager component.
"""

import json
import time
from datetime import datetime, timezone

import pytest
from common import cmdDispatch, proves_send_and_assert_command
from fprime.common.models.serialize.numerical_types import U32Type
from fprime.common.models.serialize.time_type import TimeType
from fprime_gds.common.data_types.ch_data import ChData
from fprime_gds.common.data_types.event_data import EventData
from fprime_gds.common.testing_fw.api import IntegrationTestAPI
from fprime_gds.common.testing_fw.predicates import event_predicate

resetManager = "ReferenceDeployment.resetManager"
rtcManager = "ReferenceDeployment.rtcManager"


@pytest.fixture(autouse=True)
def set_now_time(fprime_test_api: IntegrationTestAPI, start_gds):
    """Fixture to set the time to test runner's time after each test"""
    yield
    fprime_test_api.send_command(f"{resetManager}.COLD_RESET")
    # Wait for system to restart after reset
    fprime_test_api.assert_event("CdhCore.version.FrameworkVersion", timeout=10)
    # Add a small delay to ensure Authenticate component is fully initialized
    time.sleep(0.5)
    # Wait for command dispatcher to be ready by sending a NO_OP command
    fprime_test_api.send_and_assert_command(
        command=f"{cmdDispatch}.CMD_NO_OP", timeout=10
    )

    # Try to set the time and check if RTC is ready
    fprime_test_api.clear_histories()

    # Send TIME_SET command and check for DeviceNotReady event
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
    fprime_test_api.send_command(f"{rtcManager}.TIME_SET", [time_data_str])

    # Wait a bit for the command to process and check for DeviceNotReady event
    device_not_ready_event = fprime_test_api.await_event(
        f"{rtcManager}.DeviceNotReady", timeout=2
    )

    if device_not_ready_event is not None:
        # RTC is not ready, perform a soft reset
        fprime_test_api.send_command(f"{resetManager}.WARM_RESET")
        # Wait for system to restart after reset
        fprime_test_api.assert_event("CdhCore.version.FrameworkVersion", timeout=10)
        # Add a small delay to ensure Authenticate component is fully initialized
        time.sleep(0.5)
        # Wait for command dispatcher to be ready by sending a NO_OP command
        fprime_test_api.send_and_assert_command(
            command=f"{cmdDispatch}.CMD_NO_OP", timeout=10
        )
        # Try setting time again after reset
        fprime_test_api.clear_histories()
        set_time(fprime_test_api)
    else:
        # RTC is ready, verify the command completed successfully
        # Wait for OpCodeDispatched and OpCodeCompleted events
        fprime_test_api.assert_event(f"{cmdDispatch}.OpCodeDispatched", timeout=2)
        fprime_test_api.assert_event(f"{cmdDispatch}.OpCodeCompleted", timeout=2)

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
    proves_send_and_assert_command(
        fprime_test_api,
        f"{rtcManager}.TIME_SET",
        [
            time_data_str,
        ],
    )


def test_01_time_set(fprime_test_api: IntegrationTestAPI, start_gds):
    """Test that we can set the time"""

    # Set time to Curiosity landing on Mars (7 minutes of terror! https://youtu.be/Ki_Af_o9Q9s)
    curiosity_landing = datetime(2012, 8, 6, 5, 17, 57, tzinfo=timezone.utc)
    set_time(fprime_test_api, curiosity_landing)

    # Fetch event data
    result: EventData = fprime_test_api.assert_event(f"{rtcManager}.TimeSet", timeout=2)

    # Fetch previously set time from event args
    event_previous_time_arg: U32Type = result.args[0]
    previously_set_time = datetime.fromtimestamp(
        event_previous_time_arg.val, tz=timezone.utc
    )

    # Ensure microseconds are included in event
    microseconds_arg: U32Type = result.args[1]
    assert 0 <= microseconds_arg.val < 100_000_000, (
        "Microseconds arg should be >= 0 and < 1 million"
    )

    # Fetch FPrime time from event
    fp_time: TimeType = result.get_time()
    event_time = datetime.fromtimestamp(fp_time.seconds, tz=timezone.utc)

    # Assert previously set time is within 30 seconds of now
    pytest.approx(previously_set_time, abs=30) == datetime.now(timezone.utc)

    # Assert event time is within 30 seconds of curiosity landing
    pytest.approx(event_time, abs=30) == curiosity_landing

    # Fetch event data
    result: EventData = fprime_test_api.assert_event(f"{rtcManager}.TimeSet", timeout=2)

    # Assert time is within 30 seconds of now
    pytest.approx(event_time, abs=30) == datetime.now(timezone.utc)


def test_02_time_incrementing(fprime_test_api: IntegrationTestAPI, start_gds):
    """Test that time increments over time"""

    # Fetch initial time
    result: ChData = fprime_test_api.assert_telemetry(
        f"{cmdDispatch}.CommandsDispatched", timeout=3
    )

    # Convert FPrime time to datetime
    fp_time: TimeType = result.time
    initial_time = datetime.fromtimestamp(fp_time.seconds, tz=timezone.utc)

    # Wait for time to increment
    fprime_test_api.clear_histories()
    time.sleep(2.0)

    # Fetch updated time
    result: ChData = fprime_test_api.assert_telemetry(
        f"{cmdDispatch}.CommandsDispatched", timeout=3
    )

    # Convert FPrime time to datetime
    fp_time: TimeType = result.time
    updated_time = datetime.fromtimestamp(fp_time.seconds, tz=timezone.utc)

    # Assert time has increased
    assert updated_time > initial_time, (
        f"Time should increase. Initial: {initial_time}, Updated: {updated_time}"
    )


def test_03_time_not_set_event(fprime_test_api: IntegrationTestAPI, start_gds):
    """Test that a TimeNotSet event is emitted when setting time with invalid data"""

    # List of events we expect to see
    events: list[event_predicate] = [
        fprime_test_api.get_event_pred(f"{rtcManager}.YearValidationFailed"),
        fprime_test_api.get_event_pred(f"{rtcManager}.MonthValidationFailed"),
        fprime_test_api.get_event_pred(f"{rtcManager}.DayValidationFailed"),
        fprime_test_api.get_event_pred(f"{rtcManager}.HourValidationFailed"),
        fprime_test_api.get_event_pred(f"{rtcManager}.MinuteValidationFailed"),
        fprime_test_api.get_event_pred(f"{rtcManager}.SecondValidationFailed"),
        fprime_test_api.get_event_pred(f"{rtcManager}.TimeNotSet"),
        fprime_test_api.get_event_pred(f"{cmdDispatch}.OpCodeDispatched"),
        fprime_test_api.get_event_pred(f"{cmdDispatch}.OpCodeError"),
    ]

    # Clear histories
    fprime_test_api.clear_histories()

    # Send command to set the time with invalid data
    time_data = dict(
        Year=0,
        Month=12345,
        Day=12345,
        Hour=12345,
        Minute=12345,
        Second=12345,
    )
    time_data_str = json.dumps(time_data)
    fprime_test_api.send_and_assert_event(
        f"{rtcManager}.TIME_SET", [time_data_str], events, timeout=10
    )


def test_04_rtc_unconfigured(fprime_test_api: IntegrationTestAPI, start_gds):
    """Test that the RTC Manager falls back to monotonic time when the RTC device is unconfigured"""

    # Send command to unconfigure the RTC device
    proves_send_and_assert_command(
        fprime_test_api,
        f"{rtcManager}.TEST_UNCONFIGURE_DEVICE",
        [],
    )

    # Fetch time
    result: ChData = fprime_test_api.assert_telemetry(
        f"{cmdDispatch}.CommandsDispatched", timeout=3
    )

    # Convert FPrime time to datetime
    fp_time: TimeType = result.time
    initial_time = datetime.fromtimestamp(fp_time.seconds, tz=timezone.utc)

    # Arbitrarily assert time is less than a year old since time should be seconds since boot now
    assert fp_time.seconds < 31_536_000, (
        "Time seconds should be less than 1 year in seconds"
    )

    # Wait for time to increment
    fprime_test_api.clear_histories()
    time.sleep(2.0)

    # Fetch updated time
    result: ChData = fprime_test_api.assert_telemetry(
        f"{cmdDispatch}.CommandsDispatched", timeout=3
    )

    # Convert FPrime time to datetime
    fp_time: TimeType = result.time
    updated_time = datetime.fromtimestamp(fp_time.seconds, tz=timezone.utc)

    # Assert time has increased
    assert updated_time > initial_time, (
        f"Time should increase. Initial: {initial_time}, Updated: {updated_time}"
    )

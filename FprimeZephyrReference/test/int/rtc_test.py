"""
rtc_test.py:

Integration tests for the RTC Manager component.
"""

import json
import os
import tempfile
import time
from datetime import datetime, timezone
from pathlib import Path

import pytest
from common import cmdDispatch, proves_send_and_assert_command
from fprime_gds.common.data_types.ch_data import ChData
from fprime_gds.common.data_types.event_data import EventData
from fprime_gds.common.logger.test_logger import TestLogger
from fprime_gds.common.models.serialize.numerical_types import U32Type
from fprime_gds.common.models.serialize.time_type import TimeType
from fprime_gds.common.testing_fw.api import IntegrationTestAPI
from fprime_gds.common.testing_fw.predicates import event_predicate
from fprime_gds.common.tools.seqgen import SeqGenException, generateSequence

resetManager = "ReferenceDeployment.resetManager"
rtcManager = "ReferenceDeployment.rtcManager"
ina219SysManager = "ReferenceDeployment.ina219SysManager"
cmdSeq = "ReferenceDeployment.cmdSeq"
payloadSeq = "ReferenceDeployment.payloadSeq"
safeModeSeq = "ReferenceDeployment.safeModeSeq"


@pytest.fixture(autouse=True)
def set_now_time(fprime_test_api: IntegrationTestAPI, start_gds):
    """Fixture to set the time to test runner's time after each test"""
    yield
    # fprime_test_api.send_command(f"{resetManager}.WARM_RESET")
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
    proves_send_and_assert_command(
        fprime_test_api,
        f"{rtcManager}.TIME_SET",
        [
            time_data_str,
        ],
    )


def uplink_sequence_and_await_completion(
    fprime_test_api: IntegrationTestAPI,
    sequence_path: str,
    destination: str,
    timeout: int = 10,
):
    """Helper function to uplink a sequence and await its completion

    We're trying to contribute this behavior back to FPrime GDS.
    Upstream PR: https://github.com/nasa/fprime-gds/pull/281
    """
    with tempfile.TemporaryDirectory() as tempdir:
        temp_bin_path = (Path(tempdir) / Path(sequence_path).name).with_suffix(".bin")
        try:
            generateSequence(
                sequence_path,
                temp_bin_path,
                fprime_test_api.dictionaries.dictionary_path,
                0xFFFF,
                cont=True,
            )
        except OSError as ose:
            msg = f"Failed to generate sequence binary from {sequence_path}: {ose}"
            fprime_test_api.__log(msg, TestLogger.RED)
            raise
        except SeqGenException as exc:
            msg = f"Failed to generate sequence binary from {sequence_path}: {exc}"
            fprime_test_api.__log(msg, TestLogger.RED)
            raise
        fprime_test_api.uplink_file(temp_bin_path, destination)
    fprime_test_api.await_event("FileReceived", timeout=timeout)


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
        f"{ina219SysManager}.Voltage", timeout=3
    )

    # Convert FPrime time to datetime
    fp_time: TimeType = result.time
    initial_time = datetime.fromtimestamp(fp_time.seconds, tz=timezone.utc)

    # Wait for time to increment
    fprime_test_api.clear_histories()
    time.sleep(2.0)

    # Fetch updated time
    result: ChData = fprime_test_api.assert_telemetry(
        f"{ina219SysManager}.Voltage", timeout=3
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


def test_04_sequence_cancellation_on_time_set(
    fprime_test_api: IntegrationTestAPI, start_gds
):
    """Test that running sequences are canceled when RTC time is set"""

    # Uplink sequence binary to spacecraft
    seq_src_path = os.path.join(os.path.dirname(__file__), "files", "no_op.seq")
    seq_dest_path = "/seq/no_op.bin"
    uplink_sequence_and_await_completion(fprime_test_api, seq_src_path, seq_dest_path)

    # Start test sequences on cmdSeq and payloadSeq
    fprime_test_api.send_and_assert_command(
        f"{cmdSeq}.CS_RUN", [seq_dest_path, "NO_BLOCK"], timeout=5
    )

    fprime_test_api.send_and_assert_command(
        f"{payloadSeq}.CS_RUN", [seq_dest_path, "NO_BLOCK"], timeout=5
    )

    # Clear histories
    fprime_test_api.clear_histories()

    start: TimeType = TimeType().set_datetime(
        datetime.now(), time_base=TimeType.TimeBase("TB_DONT_CARE")
    )

    # Set the RTC time - this should trigger sequence cancellation
    curiosity_landing = datetime(2012, 8, 6, 5, 17, 57, tzinfo=timezone.utc)
    set_time(fprime_test_api, curiosity_landing)

    # Assert that we see CS_SequenceCanceled
    fprime_test_api.await_event(
        fprime_test_api.get_event_pred(f"{cmdSeq}.CS_SequenceCanceled"),
        start=start,
    )
    fprime_test_api.await_event(
        fprime_test_api.get_event_pred(f"{payloadSeq}.CS_SequenceCanceled"),
        start=start,
    )

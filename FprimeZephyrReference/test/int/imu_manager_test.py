"""
imu_manager_test.py:

Integration tests for the IMU Manager component.
"""

from datetime import datetime

import pytest
from common import proves_send_and_assert_command
from fprime.common.models.serialize.time_type import TimeType
from fprime_gds.common.data_types.ch_data import ChData
from fprime_gds.common.testing_fw.api import IntegrationTestAPI
from wdk import lis2mdl_manager, lsm6dso_manager


@pytest.fixture(autouse=True)
def send_packet(fprime_test_api: IntegrationTestAPI, start_gds):
    """Fixture to clear histories and send the IMU packet before each test"""
    proves_send_and_assert_command(
        fprime_test_api,
        "CdhCore.tlmSend.SEND_PKT",
        ["6"],
    )


def test_01_imu_telemetry(fprime_test_api: IntegrationTestAPI, start_gds):
    """Test that we can get IMU telemetry"""
    start: TimeType = TimeType().set_datetime(datetime.now())
    result_acceleration: ChData = fprime_test_api.assert_telemetry(
        f"{lsm6dso_manager}.Acceleration", start=start, timeout=65
    )
    result_angular_velocity: ChData = fprime_test_api.assert_telemetry(
        f"{lsm6dso_manager}.AngularVelocity", start=start, timeout=65
    )
    result_temperature: ChData = fprime_test_api.assert_telemetry(
        f"{lsm6dso_manager}.Temperature", start=start, timeout=65
    )
    result_magnetic_field: ChData = fprime_test_api.assert_telemetry(
        f"{lis2mdl_manager}.MagneticField", start=start, timeout=65
    )

    reading_acceleration: dict[float] = result_acceleration.get_val()
    reading_angular_velocity: dict[float] = result_angular_velocity.get_val()
    reading_temperature: int = result_temperature.get_val()
    reading_magnetic_field: dict[float] = result_magnetic_field.get_val()

    assert all(reading_acceleration[k] != 0 for k in ("x", "y", "z")), (
        "Acceleration reading should be non-zero"
    )
    assert all(reading_angular_velocity[k] != 0 for k in ("x", "y", "z")), (
        "AngularVelocity reading should be non-zero"
    )
    assert reading_temperature != 0, "Temperature reading should be non-zero"
    assert all(reading_magnetic_field[k] != 0 for k in ("x", "y", "z")), (
        "MagneticField reading should be non-zero"
    )

"""
imu_manager_test.py:

Integration tests for the IMU Manager component.
"""

import pytest
from common import proves_send_and_assert_command
from fprime_gds.common.data_types.ch_data import ChData
from fprime_gds.common.testing_fw.api import IntegrationTestAPI

lsm6dsoManager = "ReferenceDeployment.lsm6dsoManager"
lis2mdlManager = "ReferenceDeployment.lis2mdlManager"


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
    result_acceleration: ChData = fprime_test_api.assert_telemetry(
        f"{lsm6dsoManager}.Acceleration", start="NOW", timeout=65
    )
    result_angular_velocity: ChData = fprime_test_api.assert_telemetry(
        f"{lsm6dsoManager}.AngularVelocity", start="NOW", timeout=65
    )
    result_temperature: ChData = fprime_test_api.assert_telemetry(
        f"{lsm6dsoManager}.Temperature", start="NOW", timeout=65
    )
    result_magnetic_field: ChData = fprime_test_api.assert_telemetry(
        f"{lis2mdlManager}.MagneticField", start="NOW", timeout=65
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

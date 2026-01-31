"""
imu_manager_test.py:

Integration tests for the IMU Manager component.
"""

import pytest
from common import proves_send_and_assert_command
from fprime_gds.common.data_types.ch_data import ChData
from fprime_gds.common.testing_fw.api import IntegrationTestAPI

imuManager = "ReferenceDeployment.imuManager"


@pytest.fixture(autouse=True)
def send_packet(fprime_test_api: IntegrationTestAPI, start_gds):
    """Fixture to clear histories and send the IMU packet before each test"""
    proves_send_and_assert_command(
        fprime_test_api,
        "CdhCore.tlmSend.SEND_PKT",
        ["7"],
    )


def test_01_acceleration_telemetry(fprime_test_api: IntegrationTestAPI, start_gds):
    """Test that we can get Acceleration telemetry"""
    result: ChData = fprime_test_api.assert_telemetry(
        f"{imuManager}.Acceleration", start="NOW", timeout=3
    )

    reading: dict[float] = result.get_val()
    assert all(reading[k] != 0 for k in ("x", "y", "z")), (
        "Acceleration reading should be non-zero"
    )


def test_02_angular_velocity_telemetry(fprime_test_api: IntegrationTestAPI, start_gds):
    """Test that we can get AngularVelocity telemetry"""
    result: ChData = fprime_test_api.assert_telemetry(
        f"{imuManager}.AngularVelocity", start="NOW", timeout=3
    )

    reading: dict[float] = result.get_val()
    assert all(reading[k] != 0 for k in ("x", "y", "z")), (
        "AngularVelocity reading should be non-zero"
    )


def test_03_magnetic_field_telemetry(fprime_test_api: IntegrationTestAPI, start_gds):
    """Test that we can get MagneticField telemetry"""
    result: ChData = fprime_test_api.assert_telemetry(
        f"{imuManager}.MagneticField", start="NOW", timeout=3
    )

    reading: dict[float] = result.get_val()
    assert all(reading[k] != 0 for k in ("x", "y", "z")), (
        "MagneticField reading should be non-zero"
    )

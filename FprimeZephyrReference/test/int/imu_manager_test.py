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


def test_01_acceleration_telemetry(fprime_test_api: IntegrationTestAPI, start_gds):
    """Test that we can get Acceleration telemetry"""
    result: ChData = fprime_test_api.assert_telemetry(
        f"{lsm6dsoManager}.Acceleration", start="NOW", timeout=3
    )

    reading: dict[float] = result.get_val()
    assert all(reading[k] != 0 for k in ("x", "y", "z")), (
        "Acceleration reading should be non-zero"
    )


def test_02_angular_velocity_telemetry(fprime_test_api: IntegrationTestAPI, start_gds):
    """Test that we can get AngularVelocity telemetry"""
    result: ChData = fprime_test_api.assert_telemetry(
        f"{lsm6dsoManager}.AngularVelocity", start="NOW", timeout=3
    )

    reading: dict[float] = result.get_val()
    assert all(reading[k] != 0 for k in ("x", "y", "z")), (
        "AngularVelocity reading should be non-zero"
    )


def test_03_temperature_telemetry(fprime_test_api: IntegrationTestAPI, start_gds):
    """Test that we can get Temperature telemetry"""
    result: ChData = fprime_test_api.assert_telemetry(
        f"{lsm6dsoManager}.Temperature", start="NOW", timeout=3
    )

    reading: int = result.get_val()
    assert reading != 0, "Temperature reading should be non-zero"


def test_04_magnetic_field_telemetry(fprime_test_api: IntegrationTestAPI, start_gds):
    """Test that we can get MagneticField telemetry"""
    result: ChData = fprime_test_api.assert_telemetry(
        f"{lis2mdlManager}.MagneticField", start="NOW", timeout=3
    )

    reading: dict[float] = result.get_val()
    assert all(reading[k] != 0 for k in ("x", "y", "z")), (
        "MagneticField reading should be non-zero"
    )

"""
imu_manager_test.py:

Integration tests for the IMU Manager component.
"""

import pytest
from fprime_gds.common.data_types.ch_data import ChData
from fprime_gds.common.testing_fw.api import IntegrationTestAPI


@pytest.fixture(autouse=True)
def send_packet(fprime_test_api: IntegrationTestAPI):
    """Fixture to clear histories and send the IMU packet before each test"""
    fprime_test_api.clear_histories()
    fprime_test_api.send_and_assert_command(
        "CdhCore.tlmSend.SEND_PKT", ["6"], max_delay=2
    )


def test_01_acceleration_telemetry(fprime_test_api: IntegrationTestAPI, start_gds):
    """Test that we can get Acceleration telemetry"""
    result: ChData = fprime_test_api.assert_telemetry(
        "ReferenceDeployment.lsm6dsoManager.Acceleration", start="NOW", timeout=3
    )

    reading: dict[float] = result.get_val()
    assert all(reading[k] != 0 for k in ("x", "y", "z")), (
        "Acceleration reading should be non-zero"
    )


def test_02_angular_velocity_telemetry(fprime_test_api: IntegrationTestAPI, start_gds):
    """Test that we can get AngularVelocity telemetry"""
    result: ChData = fprime_test_api.assert_telemetry(
        "ReferenceDeployment.lsm6dsoManager.AngularVelocity", start="NOW", timeout=3
    )

    reading: dict[float] = result.get_val()
    assert all(reading[k] != 0 for k in ("x", "y", "z")), (
        "AngularVelocity reading should be non-zero"
    )


def test_03_temperature_telemetry(fprime_test_api: IntegrationTestAPI, start_gds):
    """Test that we can get Temperature telemetry"""
    result: ChData = fprime_test_api.assert_telemetry(
        "ReferenceDeployment.lsm6dsoManager.Temperature", start="NOW", timeout=3
    )

    reading: int = result.get_val()
    assert reading != 0, "Temperature reading should be non-zero"


def test_04_magnetic_field_telemetry(fprime_test_api: IntegrationTestAPI, start_gds):
    """Test that we can get MagneticField telemetry"""
    result: ChData = fprime_test_api.assert_telemetry(
        "ReferenceDeployment.lis2mdlManager.MagneticField", start="NOW", timeout=3
    )

    reading: dict[float] = result.get_val()
    assert all(reading[k] != 0 for k in ("x", "y", "z")), (
        "MagneticField reading should be non-zero"
    )

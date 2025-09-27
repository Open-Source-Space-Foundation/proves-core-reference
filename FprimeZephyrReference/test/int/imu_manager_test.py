"""
imu_manager_test.py:

Simple integration tests for the Watchdog component.
Tests are ordered so that stop tests run last.
"""

import pytest


@pytest.fixture(autouse=True)
def send_packet(fprime_test_api):
    """Fixture to clear histories and send the IMU packet before each test"""
    fprime_test_api.clear_histories()
    fprime_test_api.send_and_assert_command("CdhCore.tlmSend.SEND_PKT", ["6"], max_delay=2)


def test_01_acceleration_telemetry(fprime_test_api):
    """Test that we can get Acceleration telemetry"""
    result = fprime_test_api.assert_telemetry(
        "ReferenceDeployment.lsm6dsoManager.Acceleration",
        start="NOW", timeout=3
    )

    reading = result.get_val()
    assert all(reading[k] != 0 for k in ("x", "y", "z")), f"Acceleration values should be non-zero, got {reading}"


def test_02_angular_velocity_telemetry(fprime_test_api):
    """Test that we can get AngularVelocity telemetry"""
    result = fprime_test_api.assert_telemetry(
        "ReferenceDeployment.lsm6dsoManager.AngularVelocity",
        start="NOW", timeout=3
    )

    reading = result.get_val()
    assert all(reading[k] != 0 for k in ("x", "y", "z")), f"Acceleration values should be non-zero, got {reading}"


def test_03_temperature_telemetry(fprime_test_api):
    """Test that we can get Temperature telemetry"""
    result = fprime_test_api.assert_telemetry(
        "ReferenceDeployment.lsm6dsoManager.Temperature",
        start="NOW", timeout=3
    )

    reading = result.get_val()
    assert result.get_val() != 0, f"Temperature reading should be non-zero"


def test_04_magnetic_field_telemetry(fprime_test_api):
    """Test that we can get MagneticField telemetry"""
    result = fprime_test_api.assert_telemetry(
        "ReferenceDeployment.lis2mdlManager.MagneticField",
        start="NOW", timeout=3
    )

    reading = result.get_val()
    assert all(reading[k] != 0 for k in ("x", "y", "z")), f"Acceleration values should be non-zero, got {reading}"

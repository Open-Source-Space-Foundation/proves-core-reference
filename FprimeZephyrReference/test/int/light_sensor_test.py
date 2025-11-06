"""
light_sensor_test.py:

Integration tests for the Light Sensor component.
"""

import pytest
from common import proves_send_and_assert_command
from fprime_gds.common.data_types.ch_data import ChData
from fprime_gds.common.testing_fw.api import IntegrationTestAPI

lightSensor = "RefrenceDeployment.lightSensor"


def send_packet(fprime_test_api: IntegrationTestAPI, start_gds):
    """Fixture to clear histories and send the Light Sensor packet before each test"""
    proves_send_and_assert_command(
        fprime_test_api,
        "CdhCore.tlmSend.SEND_PKT",
        ["10"],
    )

def test_01_als_telemetry(fprime_test_api: IntegrationTestAPI, start_gds):
    """Test that we can get Ambient Light telemetry"""
    result: ChData = fprime_test_api.assert_telemetry(
        f"{lightSensor}.als", start="NOW", timeout=3
    )

    reading: dict[float] = result.get_val()
    assert als_reading != 0, "Ambient Light reading should be non-zero"

def test_02_ir_telemetry(fprime_test_api: IntegrationTestAPI, start_gds):
    """Test that we can get infared telemetry"""
    result: ChData = fprime_test_api.assert_telemetry(
        f"{lightSensor}.ir", start="NOW", timeout=3
    )

    reading: dict[float] = result.get_val()
    assert reading != 0, "Infared reading should be non-zero"

def test_03_light_telemetry(fprime_test_api: IntegrationTestAPI, start_gds):
    """Test that we can get Light telemetry"""
    result: ChData = fprime_test_api.assert_telemetry(
        f"{lightSensor}.light", start="NOW", timeout=3
    )

    reading: dict[float] = result.get_val()
    assert reading != 0, "Light reading should be non-zero"
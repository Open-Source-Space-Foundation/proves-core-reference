"""
i2c_bus_monitor_test.py:

Integration tests for the I2C0 bus monitor.
"""

import time

import pytest
from common import proves_send_and_assert_command
from fprime_gds.common.testing_fw.api import IntegrationTestAPI

RD = "ReferenceDeployment"


def send_and_assert_event(api: IntegrationTestAPI, command: str, event: str):
    """Send a command and assert the event that it causes"""
    proves_send_and_assert_command(api, f"{RD}.{command}")
    return api.assert_event(f"{RD}.{event}", timeout=2)


def test_01_trunk_works_after_recovery(fprime_test_api: IntegrationTestAPI, start_gds):
    """RECOVER_BUS succeeds and the INA219 on the I2C0 trunk answers after it"""
    send_and_assert_event(
        fprime_test_api, "i2c0BusMonitor.RECOVER_BUS", "i2c0BusMonitor.BusRecovered"
    )
    result = send_and_assert_event(
        fprime_test_api,
        "ina219SysManager.GetVoltage",
        "ina219SysManager.VoltageReading",
    )
    assert result.args[0].val > 0.0


@pytest.mark.requires_face
def test_02_mux_channel_works_after_recovery(
    fprime_test_api: IntegrationTestAPI, start_gds
):
    """A face sensor behind the TCA9548A answers after recovery"""
    proves_send_and_assert_command(fprime_test_api, f"{RD}.face0LoadSwitch.TURN_ON")
    # Face managers reject reads for 1 s after the load switch turns on
    time.sleep(1.5)
    send_and_assert_event(
        fprime_test_api, "i2c0BusMonitor.RECOVER_BUS", "i2c0BusMonitor.BusRecovered"
    )
    send_and_assert_event(
        fprime_test_api,
        "veml6031Face0Manager.GetVisibleLight",
        "veml6031Face0Manager.VisibleLight",
    )

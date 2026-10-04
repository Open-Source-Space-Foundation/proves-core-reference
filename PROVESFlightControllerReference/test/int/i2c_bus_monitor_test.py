"""
i2c_bus_monitor_test.py:

Integration tests for the I2C0 bus monitor (TCA9548A reset + 9 SCL clocks + STOP recovery).
"""

import time
from datetime import datetime

import pytest
from common import proves_send_and_assert_command
from fprime_gds.common.models.serialize.time_type import TimeType
from fprime_gds.common.testing_fw.api import IntegrationTestAPI

i2c0BusMonitor = "ReferenceDeployment.i2c0BusMonitor"
ina219SysManager = "ReferenceDeployment.ina219SysManager"
veml6031Face0Manager = "ReferenceDeployment.veml6031Face0Manager"
face0LoadSwitch = "ReferenceDeployment.face0LoadSwitch"


def now() -> TimeType:
    return TimeType().set_datetime(
        datetime.now(), time_base=TimeType.TimeBase("TB_DONT_CARE")
    )


def test_01_forced_recovery(fprime_test_api: IntegrationTestAPI, start_gds):
    """A forced recovery on an idle bus reports success with both lines high"""
    start = now()
    proves_send_and_assert_command(fprime_test_api, f"{i2c0BusMonitor}.RECOVER_BUS")
    result = fprime_test_api.assert_event(
        f"{i2c0BusMonitor}.BusRecovered", start=start, timeout=2
    )
    sda, scl = (arg.val for arg in result.args)
    assert sda == 1 and scl == 1


def test_02_trunk_works_after_recovery(fprime_test_api: IntegrationTestAPI, start_gds):
    """Devices on the I2C0 trunk (INA219) answer right after a recovery"""
    proves_send_and_assert_command(fprime_test_api, f"{i2c0BusMonitor}.RECOVER_BUS")
    start = now()
    proves_send_and_assert_command(fprime_test_api, f"{ina219SysManager}.GetVoltage")
    result = fprime_test_api.assert_event(
        f"{ina219SysManager}.VoltageReading", start=start, timeout=2
    )
    assert result.args[0].val > 0.0


@pytest.mark.requires_face
def test_03_mux_channel_works_after_recovery(
    fprime_test_api: IntegrationTestAPI, start_gds
):
    """A face sensor behind the TCA9548A answers after the mux is reset"""
    proves_send_and_assert_command(fprime_test_api, f"{face0LoadSwitch}.TURN_ON")
    # Face managers reject reads for 1 s after the load switch turns on
    time.sleep(1.5)

    proves_send_and_assert_command(fprime_test_api, f"{i2c0BusMonitor}.RECOVER_BUS")
    start = now()
    proves_send_and_assert_command(
        fprime_test_api, f"{veml6031Face0Manager}.GetVisibleLight"
    )
    fprime_test_api.assert_event(
        f"{veml6031Face0Manager}.VisibleLight", start=start, timeout=2
    )

"""
power_monitor_test.py:

Integration tests for the Power Monitor component.
"""

from datetime import datetime

import pytest
from common import proves_send_and_assert_command
from fprime.common.models.serialize.time_type import TimeType
from fprime_gds.common.data_types.ch_data import ChData
from fprime_gds.common.testing_fw.api import IntegrationTestAPI

ina219SysManager = "ReferenceDeployment.ina219SysManager"
ina219SolManager = "ReferenceDeployment.ina219SolManager"


@pytest.fixture(autouse=True)
def send_packet(fprime_test_api: IntegrationTestAPI, start_gds):
    """Fixture to send the power manager packet before each test"""
    proves_send_and_assert_command(
        fprime_test_api,
        "CdhCore.tlmSend.SEND_PKT",
        ["9"],
    )


def test_01_power_manager_telemetry(fprime_test_api: IntegrationTestAPI, start_gds):
    """Test that we can get Acceleration telemetry"""
    start: TimeType = TimeType().set_datetime(datetime.now())
    sys_voltage: ChData = fprime_test_api.assert_telemetry(
        f"{ina219SysManager}.Voltage", start=start, timeout=65
    )
    sys_current: ChData = fprime_test_api.assert_telemetry(
        f"{ina219SysManager}.Current", start=start, timeout=65
    )
    _: ChData = fprime_test_api.assert_telemetry(
        f"{ina219SysManager}.Power", start=start, timeout=65
    )
    sol_voltage: ChData = fprime_test_api.assert_telemetry(
        f"{ina219SolManager}.Voltage", start=start, timeout=65
    )
    sol_current: ChData = fprime_test_api.assert_telemetry(
        f"{ina219SolManager}.Current", start=start, timeout=65
    )
    _: ChData = fprime_test_api.assert_telemetry(
        f"{ina219SolManager}.Power", start=start, timeout=65
    )

    sys_voltage_reading: dict[float] = sys_voltage.get_val()
    sys_current_reading: dict[float] = sys_current.get_val()
    # sys_power_reading: dict[float] = sys_power.get_val()
    sol_voltage_reading: dict[float] = sol_voltage.get_val()
    sol_current_reading: dict[float] = sol_current.get_val()
    # sol_power_reading: dict[float] = sol_power.get_val()

    assert sys_voltage_reading != 0, "Acceleration reading should be non-zero"
    assert sys_current_reading != 0, "Acceleration reading should be non-zero"
    # assert sys_power_reading != 0, "Acceleration reading should be non-zero"
    assert sol_voltage_reading != 0, "Acceleration reading should be non-zero"
    assert sol_current_reading != 0, "Acceleration reading should be non-zero"
    # assert sol_power_reading != 0, "Acceleration reading should be non-zero"

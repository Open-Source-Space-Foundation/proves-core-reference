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
powerMonitor = "ReferenceDeployment.powerMonitor"


@pytest.fixture(autouse=True)
def send_packet(fprime_test_api: IntegrationTestAPI, start_gds):
    """Fixture to send the power manager packet before each test"""
    proves_send_and_assert_command(
        fprime_test_api,
        "CdhCore.tlmSend.SEND_PKT",
        ["11"],
    )
    proves_send_and_assert_command(
        fprime_test_api,
        "CdhCore.tlmSend.SEND_PKT",
        ["1"],
    )


def test_01_power_manager_telemetry(fprime_test_api: IntegrationTestAPI, start_gds):
    """Test that we can get power telemetry from INA219 managers"""
    start: TimeType = TimeType().set_datetime(
        datetime.now(), time_base=TimeType.TimeBase("TB_DONT_CARE")
    )
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
    _: ChData = fprime_test_api.assert_telemetry(
        f"{ina219SolManager}.Current", start=start, timeout=65
    )
    _: ChData = fprime_test_api.assert_telemetry(
        f"{ina219SolManager}.Power", start=start, timeout=65
    )

    # TODO: Fix the power readings once INA219 power calculation is verified
    sys_voltage_reading: dict[float] = sys_voltage.get_val()
    sys_current_reading: dict[float] = sys_current.get_val()
    # sys_power_reading: dict[float] = sys_power.get_val()
    sol_voltage_reading: dict[float] = sol_voltage.get_val()
    # sol_current_reading: dict[float] = sol_current.get_val()
    # sol_power_reading: dict[float] = sol_power.get_val()

    assert sys_voltage_reading != 0, "System voltage reading should be non-zero"
    assert sys_current_reading != 0, "System current reading should be non-zero"
    # assert sys_power_reading != 0, "System power reading should be non-zero"
    assert sol_voltage_reading != 0, "Solar voltage reading should be non-zero"
    # Solar current can be 0.0 in valid scenarios (no sunlight, etc.)
    # Existence is already verified by assert_telemetry() above
    # assert sol_current_reading != 0, "Solar current reading should be non-zero"
    # assert sol_power_reading != 0, "Solar power reading should be non-zero"


def test_02_total_power_consumption_telemetry(
    fprime_test_api: IntegrationTestAPI, start_gds
):
    """Test that TotalPowerConsumption telemetry is being updated"""
    total_power: ChData = fprime_test_api.assert_telemetry(
        f"{powerMonitor}.TotalPowerConsumption", start="NOW", timeout=65
    )

    total_power_reading: float = total_power.get_val()

    # Total power should be non-zero (accumulating over time)
    assert total_power_reading != 0, "Total power consumption should be non-zero"

"""
power_monitor_test.py:

Integration tests for the Power Monitor component.
"""

from datetime import datetime
import time

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
        ["9"],
    )


def test_01_power_manager_telemetry(fprime_test_api: IntegrationTestAPI, start_gds):
    """Test that we can get power telemetry from INA219 managers"""
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

    assert sys_voltage_reading != 0, "System voltage reading should be non-zero"
    assert sys_current_reading != 0, "System current reading should be non-zero"
    # assert sys_power_reading != 0, "System power reading should be non-zero"
    assert sol_voltage_reading != 0, "Solar voltage reading should be non-zero"
    assert sol_current_reading != 0, "Solar current reading should be non-zero"
    # assert sol_power_reading != 0, "Solar power reading should be non-zero"


def test_02_total_power_consumption_telemetry(
    fprime_test_api: IntegrationTestAPI, start_gds
):
    """Test that TotalPowerConsumption telemetry is being updated"""
    start: TimeType = TimeType().set_datetime(datetime.now())

    # Wait for a few telemetry updates (rate group runs at 1Hz)
    time.sleep(3)

    total_power: ChData = fprime_test_api.assert_telemetry(
        f"{powerMonitor}.TotalPowerConsumption", start=start, timeout=10
    )

    total_power_reading: float = total_power.get_val()

    # Total power should be non-negative (accumulating over time)
    assert total_power_reading >= 0, "Total power consumption should be non-negative"


def test_03_reset_total_power_command(
    fprime_test_api: IntegrationTestAPI, start_gds
):
    """Test that RESET_TOTAL_POWER command resets accumulated energy"""
    start: TimeType = TimeType().set_datetime(datetime.now())

    # Wait for some power to accumulate
    time.sleep(3)

    # Get current total power
    total_power_before: ChData = fprime_test_api.assert_telemetry(
        f"{powerMonitor}.TotalPowerConsumption", start=start, timeout=10
    )

    # Reset total power
    proves_send_and_assert_command(
        fprime_test_api,
        f"{powerMonitor}.RESET_TOTAL_POWER",
        [],
    )

    # Verify event was logged
    fprime_test_api.assert_event(
        f"{powerMonitor}.TotalPowerReset", start=start, timeout=10
    )

    # Wait for next telemetry update
    time.sleep(2)

    # Get total power after reset
    reset_time: TimeType = TimeType().set_datetime(datetime.now())
    total_power_after: ChData = fprime_test_api.assert_telemetry(
        f"{powerMonitor}.TotalPowerConsumption", start=reset_time, timeout=10
    )

    total_power_after_reading: float = total_power_after.get_val()

    # After reset, total power should be very small (close to 0)
    # Allow small value due to time between reset and next telemetry update
    assert total_power_after_reading < 0.1, (
        f"Total power after reset should be near 0, got {total_power_after_reading} mWh"
    )

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
    total_power: ChData = fprime_test_api.assert_telemetry(
        f"{powerMonitor}.TotalPowerConsumption", start="NOW", timeout=3
    )

    total_power_reading: float = total_power.get_val()

    # Total power should be non-negative (accumulating over time)
    assert total_power_reading >= 0, "Total power consumption should be non-negative"


def test_03_reset_total_power_command(
    fprime_test_api: IntegrationTestAPI, start_gds
):
    """Test that RESET_TOTAL_POWER command resets accumulated energy"""
    # Wait for some power to accumulate
    time.sleep(3)

    # Reset total power
    proves_send_and_assert_command(
        fprime_test_api,
        f"{powerMonitor}.RESET_TOTAL_POWER",
        [],
    )

    # Verify event was logged
    fprime_test_api.assert_event(
        f"{powerMonitor}.TotalPowerReset", start="NOW", timeout=3
    )

    # Wait for next telemetry update
    time.sleep(2)

    # Get total power after reset - should be very small (close to 0)
    # Allow small value due to time between reset and next telemetry update
    total_power_after: ChData = fprime_test_api.assert_telemetry(
        f"{powerMonitor}.TotalPowerConsumption", start="NOW", timeout=3
    )

    total_power_after_reading: float = total_power_after.get_val()

    # After reset and 2 seconds of accumulation, power should still be very small
    # At 10W total power, 2 seconds = 0.0056 mWh, so 0.01 mWh is a reasonable threshold
    assert total_power_after_reading < 0.01, (
        f"Total power after reset should be near 0, got {total_power_after_reading} mWh"
    )

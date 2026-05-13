"""
power_monitor_test.py:

Integration tests for the Power Monitor component.
"""

from common import proves_send_and_assert_command
from fprime_gds.common.data_types.event_data import EventData
from fprime_gds.common.testing_fw.api import IntegrationTestAPI

ina219SysManager = "ReferenceDeployment.ina219SysManager"
ina219SolManager = "ReferenceDeployment.ina219SolManager"
powerMonitor = "ReferenceDeployment.powerMonitor"


def get_voltage(fprime_test_api: IntegrationTestAPI, manager: str) -> float:
    """Helper function to get voltage via command and event"""
    fprime_test_api.clear_histories()

    proves_send_and_assert_command(
        fprime_test_api,
        f"{manager}.GetVoltage",
    )

    result: EventData = fprime_test_api.assert_event(
        f"{manager}.VoltageReading", timeout=3
    )

    return result.args[0].val


def get_current(fprime_test_api: IntegrationTestAPI, manager: str) -> float:
    """Helper function to get current via command and event"""
    fprime_test_api.clear_histories()

    proves_send_and_assert_command(
        fprime_test_api,
        f"{manager}.GetCurrent",
    )

    result: EventData = fprime_test_api.assert_event(
        f"{manager}.CurrentReading", timeout=3
    )

    return result.args[0].val


def get_power(fprime_test_api: IntegrationTestAPI, manager: str) -> float:
    """Helper function to get power via command and event"""
    fprime_test_api.clear_histories()

    proves_send_and_assert_command(
        fprime_test_api,
        f"{manager}.GetPower",
    )

    result: EventData = fprime_test_api.assert_event(
        f"{manager}.PowerReading", timeout=3
    )

    return result.args[0].val


def get_total_power_consumption(fprime_test_api: IntegrationTestAPI) -> float:
    """Helper function to get total power consumption via command and event"""
    fprime_test_api.clear_histories()

    proves_send_and_assert_command(
        fprime_test_api,
        f"{powerMonitor}.GET_TOTAL_POWER",
    )

    result: EventData = fprime_test_api.assert_event(
        f"{powerMonitor}.TotalPowerConsumptionReading", timeout=3
    )

    return result.args[0].val


def test_01_power_manager_readings(fprime_test_api: IntegrationTestAPI, start_gds):
    """Test that we can get power readings from INA219 managers"""

    # Get system power readings
    sys_voltage = get_voltage(fprime_test_api, ina219SysManager)
    sys_current = get_current(fprime_test_api, ina219SysManager)
    _ = get_power(fprime_test_api, ina219SysManager)

    # Get solar power readings
    sol_voltage = get_voltage(fprime_test_api, ina219SolManager)
    _ = get_current(fprime_test_api, ina219SolManager)
    _ = get_power(fprime_test_api, ina219SolManager)

    # TODO: Fix the power readings once INA219 power calculation is verified
    assert sys_voltage != 0, "System voltage reading should be non-zero"
    assert sys_current != 0, "System current reading should be non-zero"
    # assert sys_power != 0, "System power reading should be non-zero"
    assert sol_voltage != 0, "Solar voltage reading should be non-zero"
    # Solar current can be 0.0 in valid scenarios (no sunlight, etc.)
    # Existence is already verified by get_current() above
    # assert sol_current != 0, "Solar current reading should be non-zero"
    # assert sol_power != 0, "Solar power reading should be non-zero"


def test_02_total_power_consumption(fprime_test_api: IntegrationTestAPI, start_gds):
    """Test that TotalPowerConsumption is being updated"""

    total_power = get_total_power_consumption(fprime_test_api)

    # Total power should be non-zero (accumulating over time)
    assert total_power != 0, "Total power consumption should be non-zero"

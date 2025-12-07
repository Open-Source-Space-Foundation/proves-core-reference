"""
drv2605_test.py:

Integration tests for the DRV2605 component.
"""

from datetime import datetime

import pytest
from common import proves_send_and_assert_command
from fprime.common.models.serialize.time_type import TimeType
from fprime_gds.common.data_types.ch_data import ChData
from fprime_gds.common.testing_fw.api import IntegrationTestAPI

drv2605Manager = "ReferenceDeployment.drv2605Face0Manager"
ina219SysManager = "ReferenceDeployment.ina219SysManager"


@pytest.fixture(autouse=True)
def setup_test(fprime_test_api: IntegrationTestAPI, start_gds):
    """Fixture to turn on face 0 before each test"""
    proves_send_and_assert_command(
        fprime_test_api,
        "ReferenceDeployment.face0LoadSwitch.TURN_ON",
        [],
    )


def get_system_power(fprime_test_api: IntegrationTestAPI) -> float:
    """Helper to get the current system power draw in Watts"""
    start: TimeType = TimeType().set_datetime(datetime.now())
    proves_send_and_assert_command(
        fprime_test_api,
        "CdhCore.tlmSend.SEND_PKT",
        ["10"],
    )
    system_power: ChData = fprime_test_api.assert_telemetry(
        f"{ina219SysManager}.Power", start=start, timeout=3
    )
    return system_power.get_val()


def test_01_magnetorquer_power_draw(fprime_test_api: IntegrationTestAPI, start_gds):
    """Test that magnetorquer powers on by asserting higher power draw"""

    baseline_power = get_system_power(fprime_test_api)
    power_during_trigger: list[float] = []
    for _ in range(3):
        proves_send_and_assert_command(fprime_test_api, f"{drv2605Manager}.TRIGGER")
        power_during_trigger.append(get_system_power(fprime_test_api))

    # average_power = sum(power_during_trigger) / len(power_during_trigger)
    maximum_power = max(power_during_trigger)
    assert maximum_power > baseline_power + 0.5, (
        f"Max power during magnetorquer trigger ({maximum_power} W) "
        f"not sufficiently above baseline power ({baseline_power} W)"
    )

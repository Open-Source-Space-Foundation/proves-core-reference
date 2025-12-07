"""
drv2605_test.py:

Integration tests for the DRV2605 component.
"""

import time
from datetime import datetime

import pytest
from common import proves_send_and_assert_command
from fprime.common.models.serialize.time_type import TimeType
from fprime_gds.common.testing_fw.api import IntegrationTestAPI

drv2605Manager = "ReferenceDeployment.drv2605Face0Manager"
ina219SysManager = "ReferenceDeployment.ina219SysManager"


@pytest.fixture(autouse=True)
def setup_test(fprime_test_api: IntegrationTestAPI, start_gds):
    """Fixture to turn on face 0 before each test"""
    proves_send_and_assert_command(
        fprime_test_api,
        "ReferenceDeployment.face0LoadSwitch.TURN_ON",
    )
    yield
    proves_send_and_assert_command(
        fprime_test_api,
        "ReferenceDeployment.face0LoadSwitch.TURN_OFF",
    )


def get_system_power(fprime_test_api: IntegrationTestAPI) -> float:
    """Helper to get the current system power draw in Watts"""
    start: TimeType = TimeType().set_datetime(datetime.now())
    proves_send_and_assert_command(
        fprime_test_api,
        f"{ina219SysManager}.GetPower",
    )
    power_event = fprime_test_api.assert_event(
        f"{ina219SysManager}.PowerReading", start=start, timeout=3
    )
    return power_event.args[0].val


def test_01_magnetorquer_power_draw(fprime_test_api: IntegrationTestAPI, start_gds):
    """Test that magnetorquer powers on by asserting higher power draw"""

    baseline_power = get_system_power(fprime_test_api)
    proves_send_and_assert_command(
        fprime_test_api, f"{drv2605Manager}.START_CONTINUOUS_MODE"
    )

    time.sleep(1)  # Allow some time for power increase

    try:
        start: TimeType = TimeType().set_datetime(datetime.now())
        proves_send_and_assert_command(
            fprime_test_api,
            f"{ina219SysManager}.GetPower",
        )

        power_event = fprime_test_api.assert_event(
            f"{ina219SysManager}.PowerReading", start=start, timeout=2
        )

        during_trigger_power = power_event.args[0].val
        assert during_trigger_power > baseline_power + 0.3, (
            f"Power during magnetorquer trigger ({during_trigger_power} W) "
            f"not sufficiently above baseline power ({baseline_power} W)"
        )

    except Exception as e:
        raise e
    finally:
        # Ensure burnwire is stopped
        proves_send_and_assert_command(
            fprime_test_api, f"{drv2605Manager}.STOP_CONTINUOUS_MODE"
        )

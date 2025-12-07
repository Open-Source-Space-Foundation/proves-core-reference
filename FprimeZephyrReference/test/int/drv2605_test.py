"""
drv2605_test.py:

Integration tests for the DRV2605 component.
"""

import time
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
    proves_send_and_assert_command(
        fprime_test_api, f"{drv2605Manager}.START_CONTINUOUS_MODE"
    )

    time.sleep(1)  # Allow some time for power increase

    try:
        proves_send_and_assert_command(
            fprime_test_api,
            "CdhCore.tlmSend.SEND_PKT",
            ["10"],
        )

        system_power: ChData = fprime_test_api.assert_telemetry(
            f"{ina219SysManager}.Power", start="NOW", timeout=2
        )

        during_trigger_power = system_power.get_val()
        assert during_trigger_power > baseline_power + 0.5, (
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

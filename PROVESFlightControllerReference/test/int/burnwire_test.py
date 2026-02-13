"""
burnwire_test.py:

Integration tests for the Burnwire component.
"""

import time

import pytest
from common import proves_send_and_assert_command
from fprime_gds.common.data_types.ch_data import ChData
from fprime_gds.common.testing_fw.api import IntegrationTestAPI

ina219SysManager = "ReferenceDeployment.ina219SysManager"

burnwire = "ReferenceDeployment.burnwire"


@pytest.fixture(autouse=True)
def reset_burnwire(fprime_test_api: IntegrationTestAPI, start_gds):
    """Fixture to stop burnwire and clear histories before/after each test"""
    # Stop burnwire and clear before test
    stop_burnwire(fprime_test_api)
    yield
    # Clear again after test to prevent residue
    stop_burnwire(fprime_test_api)


def stop_burnwire(fprime_test_api: IntegrationTestAPI):
    """Stop the burnwire and clear histories"""
    proves_send_and_assert_command(fprime_test_api, f"{burnwire}.STOP_BURNWIRE")

    fprime_test_api.assert_event(f"{burnwire}.SetBurnwireState", "OFF", timeout=15)

    fprime_test_api.assert_event(f"{burnwire}.BurnwireEndCount", timeout=2)


def test_01_start_and_stop_burnwire(fprime_test_api: IntegrationTestAPI, start_gds):
    """Test that burnwire starts and stops as expected"""

    # Start burnwire
    proves_send_and_assert_command(fprime_test_api, f"{burnwire}.START_BURNWIRE")

    # Wait for SetBurnwireState = ON
    fprime_test_api.assert_event(f"{burnwire}.SetBurnwireState", "ON", timeout=2)

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

        assert system_power.get_val() > 1, (
            "System power should be greater than 1 Watt when burnwire is ON"
        )

    except Exception as e:
        raise e
    finally:
        # Ensure burnwire is stopped
        proves_send_and_assert_command(fprime_test_api, f"{burnwire}.STOP_BURNWIRE")

    # Confirm Burnwire turned OFF
    fprime_test_api.assert_event(f"{burnwire}.SetBurnwireState", "OFF", timeout=2)

    fprime_test_api.assert_event(f"{burnwire}.BurnwireEndCount", timeout=2)

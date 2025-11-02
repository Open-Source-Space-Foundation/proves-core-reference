"""
watchdog_test.py:

Integration tests for the Watchdog component.
"""

import time

import pytest
from common import proves_send_and_assert_command
from fprime_gds.common.data_types.ch_data import ChData
from fprime_gds.common.testing_fw.api import IntegrationTestAPI

watchdog = "ReferenceDeployment.watchdog"


@pytest.fixture(autouse=True)
def ensure_watchdog_running(fprime_test_api: IntegrationTestAPI, start_gds):
    """Fixture to ensure watchdog is started before and after each test"""
    start_watchdog(fprime_test_api)
    yield
    start_watchdog(fprime_test_api)


def start_watchdog(fprime_test_api: IntegrationTestAPI):
    """Helper function to start the watchdog"""
    proves_send_and_assert_command(
        fprime_test_api,
        f"{watchdog}.START_WATCHDOG",
    )


def get_watchdog_transitions(fprime_test_api: IntegrationTestAPI) -> int:
    """Helper function to request packet and get fresh WatchdogTransitions telemetry"""
    proves_send_and_assert_command(
        fprime_test_api,
        "CdhCore.tlmSend.SEND_PKT",
        ["5"],
    )
    result: ChData = fprime_test_api.assert_telemetry(
        f"{watchdog}.WatchdogTransitions", start="NOW", timeout=65
    )
    return result.get_val()


def test_01_watchdog_behavior(fprime_test_api: IntegrationTestAPI, start_gds):
    """Test the behavior of the Watchdog component"""
    initial_value = get_watchdog_transitions(fprime_test_api)
    assert initial_value >= 0, (
        f"WatchdogTransitions should be >= 0, got {initial_value}"
    )

    # Wait for watchdog to run more cycles
    time.sleep(2.0)

    # Send stop command
    proves_send_and_assert_command(
        fprime_test_api,
        f"{watchdog}.STOP_WATCHDOG",
    )

    # Check that transitions incremented before stopping
    updated_value = get_watchdog_transitions(fprime_test_api)
    assert updated_value > initial_value, (
        f"WatchdogTransitions should increase. Initial: {initial_value}, Updated: {updated_value}"
    )

    # Check that it stops incrementing
    final_value = get_watchdog_transitions(fprime_test_api)
    assert final_value == updated_value, (
        f"Watchdog should remain stopped. Updated: {updated_value}, Final: {final_value}"
    )

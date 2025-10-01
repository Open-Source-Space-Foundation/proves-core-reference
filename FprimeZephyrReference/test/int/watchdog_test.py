"""
watchdog_test.py:

Integration tests for the Watchdog component.
"""

import time

import pytest
from fprime_gds.common.data_types.ch_data import ChData
from fprime_gds.common.testing_fw.api import IntegrationTestAPI


@pytest.fixture(autouse=True)
def ensure_watchdog_running(fprime_test_api: IntegrationTestAPI):
    """Fixture to ensure watchdog is started before and after each test"""
    start_watchdog(fprime_test_api)
    yield
    start_watchdog(fprime_test_api)


def start_watchdog(fprime_test_api: IntegrationTestAPI):
    """Helper function to start the watchdog"""
    fprime_test_api.send_and_assert_command(
        "ReferenceDeployment.watchdog.START_WATCHDOG", max_delay=2
    )
    fprime_test_api.assert_event(
        "ReferenceDeployment.watchdog.WatchdogStart", timeout=2
    )


def get_watchdog_transitions(fprime_test_api: IntegrationTestAPI) -> int:
    """Helper function to request packet and get fresh WatchdogTransitions telemetry"""
    fprime_test_api.clear_histories()
    fprime_test_api.send_and_assert_command(
        "CdhCore.tlmSend.SEND_PKT", ["5"], max_delay=2
    )
    result: ChData = fprime_test_api.assert_telemetry(
        "ReferenceDeployment.watchdog.WatchdogTransitions", start="NOW", timeout=3
    )
    return result.get_val()


def test_01_watchdog_telemetry_basic(fprime_test_api: IntegrationTestAPI):
    """Test that we can read WatchdogTransitions telemetry"""
    value = get_watchdog_transitions(fprime_test_api)
    assert value >= 0, f"WatchdogTransitions should be >= 0, got {value}"


def test_02_watchdog_increments(fprime_test_api: IntegrationTestAPI):
    """Test that WatchdogTransitions increments over time"""

    initial_value = get_watchdog_transitions(fprime_test_api)
    time.sleep(2.0)  # Wait for watchdog to run more cycles
    updated_value = get_watchdog_transitions(fprime_test_api)

    assert updated_value > initial_value, (
        f"WatchdogTransitions should increase. Initial: {initial_value}, Updated: {updated_value}"
    )


def test_03_stop_watchdog_command(fprime_test_api: IntegrationTestAPI):
    """
    Test STOP_WATCHDOG command sends and emits WatchdogStop
    event and WatchdogTransitions stops incrementing
    """
    fprime_test_api.clear_histories()

    # Send stop command
    fprime_test_api.send_and_assert_command(
        "ReferenceDeployment.watchdog.STOP_WATCHDOG", max_delay=2
    )

    # Check for watchdog stop event
    fprime_test_api.assert_event("ReferenceDeployment.watchdog.WatchdogStop", timeout=2)

    # Get watchdog transition count
    initial_value = get_watchdog_transitions(fprime_test_api)

    # Wait and check that it's no longer incrementing
    time.sleep(2.0)
    final_value = get_watchdog_transitions(fprime_test_api)

    assert final_value == initial_value, (
        f"Watchdog should remain stopped. Initial: {initial_value}, Final: {final_value}"
    )

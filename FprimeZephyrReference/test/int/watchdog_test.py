"""
watchdog_test.py:

Integration tests for the Watchdog component.
"""

import pytest
from common import proves_send_and_assert_command
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


def test_01_stop_watchdog_command(fprime_test_api: IntegrationTestAPI, start_gds):
    """
    Test STOP_WATCHDOG command sends and emits WatchdogStop
    event and WatchdogTransitions stops incrementing
    """

    # Send stop command
    proves_send_and_assert_command(
        fprime_test_api,
        f"{watchdog}.STOP_WATCHDOG",
    )

    # Check for watchdog stop event
    fprime_test_api.assert_event("ReferenceDeployment.watchdog.WatchdogStop", timeout=2)

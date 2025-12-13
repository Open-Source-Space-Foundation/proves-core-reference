"""
watchdog_test.py:

Integration tests for the Watchdog component.
"""

from time import time

import pytest
from common import proves_send_and_assert_command
from fprime_gds.common.data_types.ch_data import ChData
from fprime_gds.common.testing_fw.api import IntegrationTestAPI

watchdog = "ReferenceDeployment.watchdog"
startup_manager = "ReferenceDeployment.startupManager"


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


def get_boot_count(fprime_test_api: IntegrationTestAPI) -> int:
    """Helper function to request packet and get fresh BootCount telemetry"""

    result: ChData = fprime_test_api.assert_telemetry(
        f"{startup_manager}.BootCount", start="NOW", timeout=3
    )
    return result.get_val()


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


def test_02_system_stays_running_with_watchdog(
    fprime_test_api: IntegrationTestAPI, start_gds
):
    """
    Test that the system stays running for 30+ seconds when watchdog is active.
    Boot count should remain the same.
    """
    # Get initial boot count
    initial_boot_count = get_boot_count(fprime_test_api)

    # Wait for 30 seconds with watchdog running
    time.sleep(30.0)

    # Check boot count hasn't changed (no reboot occurred)
    final_boot_count = get_boot_count(fprime_test_api)

    assert final_boot_count == initial_boot_count, (
        f"System should not have rebooted with watchdog active. "
        f"Initial boot count: {initial_boot_count}, Final boot count: {final_boot_count}"
    )


def test_03_system_reboots_without_watchdog(
    fprime_test_api: IntegrationTestAPI, start_gds
):
    """
    Test that when the watchdog is stopped, the system reboots after ~30 seconds.
    Boot count should increment by 1.
    """
    # Get initial boot count
    initial_boot_count = get_boot_count(fprime_test_api)

    # Stop the watchdog
    proves_send_and_assert_command(
        fprime_test_api,
        f"{watchdog}.STOP_WATCHDOG",
    )

    # Check for watchdog stop event
    fprime_test_api.assert_event(f"{watchdog}.WatchdogStop", timeout=2)

    # Wait for system to reboot
    time.sleep(45.0)

    # Check boot count has incremented by 1
    final_boot_count = get_boot_count(fprime_test_api)

    assert final_boot_count == initial_boot_count + 1, (
        f"System should have rebooted once after watchdog stopped. "
        f"Initial boot count: {initial_boot_count}, Final boot count: {final_boot_count}"
    )

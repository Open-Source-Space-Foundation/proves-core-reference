"""
burnwire_test.py:

Integration tests for the Burnwire component.
"""

import pytest
from common import proves_send_and_assert_command
from fprime_gds.common.testing_fw.api import IntegrationTestAPI

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

    fprime_test_api.assert_event(f"{burnwire}.SetBurnwireState", "OFF", timeout=10)

    fprime_test_api.assert_event(f"{burnwire}.BurnwireEndCount", timeout=2)


def test_01_start_and_stop_burnwire(fprime_test_api: IntegrationTestAPI, start_gds):
    """Test that burnwire starts and stops as expected"""

    # Start burnwire
    proves_send_and_assert_command(fprime_test_api, f"{burnwire}.START_BURNWIRE")

    # Wait for SetBurnwireState = ON
    fprime_test_api.assert_event(f"{burnwire}.SetBurnwireState", "ON", timeout=2)

    fprime_test_api.assert_event(f"{burnwire}.SafetyTimerState", timeout=2)

    fprime_test_api.assert_event(f"{burnwire}.SetBurnwireState", "OFF", timeout=10)

    fprime_test_api.assert_event(f"{burnwire}.BurnwireEndCount", timeout=2)


def test_02_manual_stop_before_timeout(fprime_test_api: IntegrationTestAPI, start_gds):
    """Test that burnwire stops manually before the safety timer expires"""

    # Start burnwire
    proves_send_and_assert_command(fprime_test_api, f"{burnwire}.START_BURNWIRE")

    # Confirm Burnwire turned ON
    fprime_test_api.assert_event(f"{burnwire}.SetBurnwireState", "ON", timeout=2)

    # # Stop burnwire before safety timer triggers
    proves_send_and_assert_command(fprime_test_api, f"{burnwire}.STOP_BURNWIRE")

    # Confirm Burnwire turned OFF
    fprime_test_api.assert_event(f"{burnwire}.SetBurnwireState", "OFF", timeout=2)

    fprime_test_api.assert_event(f"{burnwire}.BurnwireEndCount", timeout=2)

# """
# burnwire_test.py:

# Integration tests for the Burnwire component.
# """

import pytest
from fprime_gds.common.testing_fw.api import IntegrationTestAPI

# Constants


@pytest.fixture(autouse=True)
def reset_burnwire(fprime_test_api: IntegrationTestAPI):
    """Fixture to stop burnwire and clear histories before/after each test"""
    # Stop burnwire and clear before test
    stop_burnwire(fprime_test_api)
    fprime_test_api.clear_histories()
    yield
    # Clear again after test to prevent residue
    stop_burnwire(fprime_test_api)
    fprime_test_api.clear_histories()


def stop_burnwire(fprime_test_api: IntegrationTestAPI):
    fprime_test_api.send_and_assert_command(
        "ReferenceDeployment.burnwire.STOP_BURNWIRE", max_delay=10
    )

    fprime_test_api.assert_event(
        "ReferenceDeployment.burnwire.SetBurnwireState", "OFF", timeout=5
    )

    fprime_test_api.assert_event(
        "ReferenceDeployment.burnwire.BurnwireEndCount", timeout=5
    )


def test_01_start_and_stop_burnwire(fprime_test_api: IntegrationTestAPI):
    """Test that burnwire starts and stops as expected"""

    # Start burnwire
    fprime_test_api.send_and_assert_command(
        "ReferenceDeployment.burnwire.START_BURNWIRE", max_delay=2
    )

    # Wait for SetBurnwireState = ON
    fprime_test_api.assert_event(
        "ReferenceDeployment.burnwire.SetBurnwireState", "ON", timeout=2
    )

    fprime_test_api.assert_event(
        "ReferenceDeployment.burnwire.SafetyTimerState", timeout=2
    )

    fprime_test_api.assert_event(
        "ReferenceDeployment.burnwire.SetBurnwireState", "OFF", timeout=10
    )

    fprime_test_api.assert_event(
        "ReferenceDeployment.burnwire.SetBurnwireState", "OFF", timeout=2
    )

    fprime_test_api.assert_event(
        "ReferenceDeployment.burnwire.BurnwireEndCount", timeout=2
    )


def test_02_manual_stop_before_timeout(fprime_test_api: IntegrationTestAPI):
    """Test that burnwire stops manually before the safety timer expires"""

    # Start burnwire
    fprime_test_api.send_and_assert_command(
        "ReferenceDeployment.burnwire.START_BURNWIRE", max_delay=2
    )

    # Confirm Burnwire turned ON
    fprime_test_api.assert_event(
        "ReferenceDeployment.burnwire.SetBurnwireState", "ON", timeout=2
    )

    # # Stop burnwire before safety timer triggers
    fprime_test_api.send_and_assert_command(
        "ReferenceDeployment.burnwire.STOP_BURNWIRE", max_delay=2
    )

    # Confirm Burnwire turned OFF
    fprime_test_api.assert_event(
        "ReferenceDeployment.burnwire.SetBurnwireState", "OFF", timeout=2
    )

    fprime_test_api.assert_event(
        "ReferenceDeployment.burnwire.SetBurnwireState", "OFF", timeout=2
    )

    fprime_test_api.assert_event(
        "ReferenceDeployment.burnwire.BurnwireEndCount", timeout=2
    )

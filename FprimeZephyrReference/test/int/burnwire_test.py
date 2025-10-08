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
    fprime_test_api.clear_histories()
    stop_burnwire(fprime_test_api)
    yield
    # Clear again after test to prevent residue
    fprime_test_api.clear_histories()
    stop_burnwire(fprime_test_api)


def stop_burnwire(fprime_test_api: IntegrationTestAPI):
    fprime_test_api.send_and_assert_command(
        "ReferenceDeployment.burnwire.STOP_BURNWIRE"
    )

    fprime_test_api.assert_event(
        "ReferenceDeployment.burnwire.SetBurnwireState", "OFF", timeout=10
    )

    fprime_test_api.assert_event(
        "ReferenceDeployment.burnwire.BurnwireEndCount", timeout=2
    )

    received_events = fprime_test_api.get_event_subhistory()
    print(f"Received events: {received_events}")


def test_01_start_and_stop_burnwire(fprime_test_api: IntegrationTestAPI, start_gds):
    """Test that burnwire starts and stops as expected"""

    # Start burnwire
    fprime_test_api.send_and_assert_command(
        "ReferenceDeployment.burnwire.START_BURNWIRE"
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
        "ReferenceDeployment.burnwire.BurnwireEndCount", timeout=2
    )


def test_02_manual_stop_before_timeout(fprime_test_api: IntegrationTestAPI, start_gds):
    """Test that burnwire stops manually before the safety timer expires"""

    # Start burnwire
    fprime_test_api.send_and_assert_command(
        "ReferenceDeployment.burnwire.START_BURNWIRE"
    )

    # Confirm Burnwire turned ON
    fprime_test_api.assert_event(
        "ReferenceDeployment.burnwire.SetBurnwireState", "ON", timeout=2
    )

    # # Stop burnwire before safety timer triggers
    fprime_test_api.send_and_assert_command(
        "ReferenceDeployment.burnwire.STOP_BURNWIRE"
    )

    # Confirm Burnwire turned OFF
    fprime_test_api.assert_event(
        "ReferenceDeployment.burnwire.SetBurnwireState", "OFF", timeout=2
    )

    fprime_test_api.assert_event(
        "ReferenceDeployment.burnwire.BurnwireEndCount", timeout=2
    )

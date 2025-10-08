# """
# burnwire_test.py:

# Integration tests for the Burnwire component.
# """


import pytest
from fprime_gds.common.data_types.event_data import EventData
from fprime_gds.common.testing_fw.api import IntegrationTestAPI

# Constants
COMPONENT = "ReferenceDeployment.burnwire"


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
        "ReferenceDeployment.burnwire.STOP_BURNWIRE", max_delay=2
    )
    fprime_test_api.assert_event(f"{COMPONENT}.SetBurnwireState", ["ON"], timeout=2)


def test_01_start_and_stop_burnwire(fprime_test_api: IntegrationTestAPI):
    """Test that burnwire starts and stops as expected"""

    # Start burnwire
    fprime_test_api.send_and_assert_command(
        f"{COMPONENT}.START_BURNWIRE", [], max_delay=2
    )

    # Wait for SetBurnwireState = ON
    fprime_test_api.assert_event(f"{COMPONENT}.SetBurnwireState", ["ON"], timeout=2)
    # assert result is not None, "Burnwire ON event not received"

    fprime_test_api.assert_event(f"{COMPONENT}.SetBurnwireState", ["OFF"], timeout=10)


def test_02_manual_stop_before_timeout(fprime_test_api: IntegrationTestAPI):
    """Test that burnwire stops manually before the safety timer expires"""

    # Start burnwire
    fprime_test_api.send_and_assert_command(
        f"{COMPONENT}.START_BURNWIRE", [], max_delay=2
    )

    # Confirm Burnwire turned ON
    result: EventData = fprime_test_api.assert_event(
        f"{COMPONENT}.SetBurnwireState", ["ON"], timeout=2, start=0
    )
    assert result is not None, "Burnwire ON event not received"

    # # Stop burnwire before safety timer triggers
    fprime_test_api.send_and_assert_command(
        f"{COMPONENT}.STOP_BURNWIRE", [], max_delay=2
    )

    # Confirm Burnwire turned OFF
    result: EventData = fprime_test_api.assert_event(
        f"{COMPONENT}.SetBurnwireState", ["OFF"], timeout=2, start=0
    )
    assert result is not None, "Burnwire OFF event not received"

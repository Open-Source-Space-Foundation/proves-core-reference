"""
load_switch_test.py:

Integration tests for the Load-Switch component.
"""

import time

import pytest
from common import proves_send_and_assert_command
from fprime_gds.common.data_types.event_data import EventData
from fprime_gds.common.testing_fw.api import IntegrationTestAPI

ON = "ON"
OFF = "OFF"

loadswitch = "ReferenceDeployment.face0LoadSwitch"


@pytest.fixture(autouse=True)
def ensure_loadswitch_off(fprime_test_api: IntegrationTestAPI, start_gds):
    """Ensure LoadSwitch starts in OFF state"""
    turn_off(fprime_test_api)
    yield
    turn_off(fprime_test_api)


def turn_on(fprime_test_api: IntegrationTestAPI):
    """Helper function to turn on the loadswitch"""
    proves_send_and_assert_command(
        fprime_test_api,
        f"{loadswitch}.TURN_ON",
    )


def turn_off(fprime_test_api: IntegrationTestAPI):
    """Helper function to turn off the loadswitch"""
    proves_send_and_assert_command(fprime_test_api, f"{loadswitch}.TURN_OFF")


def get_is_on(fprime_test_api: IntegrationTestAPI) -> str:
    """Helper function to get load switch state via command and event"""
    fprime_test_api.clear_histories()

    proves_send_and_assert_command(
        fprime_test_api,
        f"{loadswitch}.GET_IS_ON",
    )

    result: EventData = fprime_test_api.assert_event(f"{loadswitch}.IsOn", timeout=3)

    return result.args[0].val


def test_01_loadswitch_telemetry_and_events(
    fprime_test_api: IntegrationTestAPI, start_gds
):
    """Test that we can read IsOn state as well as a toggle on / off event."""
    value = get_is_on(fprime_test_api)
    assert value in (ON, OFF), f"IsOn should be {ON} or {OFF}, got {value}"

    time.sleep(1)

    # Send turn_on command
    turn_on(fprime_test_api)

    # Confirm Load-Switch turned ON
    fprime_test_api.assert_event(f"{loadswitch}.StatusChanged", args=[ON], timeout=2)

    # Confirm IsOn is ON
    value = get_is_on(fprime_test_api)
    assert value == ON, f"Expected IsOn = {ON} after TURN_ON, got {value}"

    # Send turn_off command
    turn_off(fprime_test_api)

    # Confirm Load-Switch turned OFF
    fprime_test_api.assert_event(f"{loadswitch}.StatusChanged", args=[OFF], timeout=2)

    # Confirm IsOn is OFF
    value = get_is_on(fprime_test_api)
    assert value == OFF, f"Expected IsOn = {OFF} after TURN_OFF, got {value}"

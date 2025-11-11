"""
load_switch_test.py:

Integration tests for the Load-Switch component.
"""

import pytest
from common import proves_send_and_assert_command
from fprime_gds.common.data_types.ch_data import ChData
from fprime_gds.common.testing_fw.api import IntegrationTestAPI

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


def get_is_on(fprime_test_api: IntegrationTestAPI) -> int:
    """Helper function to request packet and get fresh IsOn telemetry"""
    proves_send_and_assert_command(
        fprime_test_api,
        "CdhCore.tlmSend.SEND_PKT",
        ["9"],
    )
    result: ChData = fprime_test_api.assert_telemetry(
        f"{loadswitch}.IsOn", start="NOW", timeout=3
    )
    return result.get_val()


def test_01_loadswitch_telemetry_basic(fprime_test_api: IntegrationTestAPI, start_gds):
    """Test that we can read IsOn telemetry"""
    value = get_is_on(fprime_test_api)
    assert value in (0, 1), f"IsOn should be 0 or 1, got {value}"


def test_02_turn_on_sets_high(fprime_test_api: IntegrationTestAPI, start_gds):
    """
    Test TURN_ON command sets GPIO high, emits ON event, and updates telemetry
    """

    # Send turn_on command
    turn_on(fprime_test_api)

    # Confirm Load-Switch turned ON
    fprime_test_api.assert_event(f"{loadswitch}.StatusChanged", args=[1], timeout=2)

    # Confirm telemetry IsOn is 1
    value = get_is_on(fprime_test_api)
    assert value == 1, f"Expected IsOn = 1 after TURN_ON, got {value}"


def test_03_turn_off_sets_low(fprime_test_api: IntegrationTestAPI, start_gds):
    """
    Test TURN_OFF command sets GPIO low, emits OFF event, and updates telemetry
    """

    # Send turn_on command
    turn_off(fprime_test_api)

    # Confirm Load-Switch turned OFF
    fprime_test_api.assert_event(f"{loadswitch}.StatusChanged", args=[0], timeout=2)

    # Confirm telemetry IsOn is 0
    value = get_is_on(fprime_test_api)
    assert value == 0, f"Expected IsOn = 0 after TURN_OFF, got {value}"

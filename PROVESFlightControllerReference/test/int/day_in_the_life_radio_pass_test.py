"""
day_in_the_life_radio_pass_test.py:

Long-duration over-radio integration test that validates sustained downlink
activity across a full ground-station pass-length window.
"""

import time

import pytest
from common import proves_send_and_assert_command
from fprime_gds.common.testing_fw.api import IntegrationTestAPI

downlinkDelay = "ReferenceDeployment.downlinkDelay"
lora = "ReferenceDeployment.lora"

WINDOW_SECONDS = 30
PASS_DURATION_SECONDS = 6 * 60
POLL_SECONDS = 1

pytestmark = [pytest.mark.day_in_the_life]


def _await_downlink_window(telemetry_subhist, event_subhist, timeout_s: int) -> bool:
    deadline = time.monotonic() + timeout_s
    while time.monotonic() < deadline:
        if telemetry_subhist.retrieve_new() or event_subhist.retrieve_new():
            return True
        time.sleep(POLL_SECONDS)
    return False


def test_radio_day_in_the_life_downlink_cadence(
    fprime_test_api: IntegrationTestAPI, start_gds
):
    """After enabling TRANSMIT, receive events or telemetry every 30s for ~6 min."""
    proves_send_and_assert_command(
        fprime_test_api,
        f"{downlinkDelay}.DIVIDER_PRM_SET",
        [20],
    )
    proves_send_and_assert_command(
        fprime_test_api,
        f"{lora}.TRANSMIT",
        ["ENABLED"],
    )

    telemetry_subhist = fprime_test_api.get_telemetry_subhistory()
    event_subhist = fprime_test_api.get_event_subhistory()

    try:
        windows = PASS_DURATION_SECONDS // WINDOW_SECONDS
        for window in range(1, windows + 1):
            assert _await_downlink_window(
                telemetry_subhist, event_subhist, WINDOW_SECONDS
            ), (
                "No event or telemetry arrived within "
                f"{WINDOW_SECONDS}s window {window}/{windows}"
            )
    finally:
        fprime_test_api.remove_telemetry_subhistory(telemetry_subhist)
        fprime_test_api.remove_event_subhistory(event_subhist)

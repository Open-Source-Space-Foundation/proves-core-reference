"""
radio_test.py:

Integration tests for the Radio.
"""

import time
from datetime import datetime

import pytest
from common import proves_send_and_assert_command
from fprime_gds.common.models.serialize.time_type import TimeType
from fprime_gds.common.testing_fw.api import IntegrationTestAPI

pytestmark = [pytest.mark.uart_only]

downlinkDelay = "ReferenceDeployment.downlinkDelay"
# v5e builds Zephyr::UspRadio (not the legacy Zephyr::LoRa component); TRANSMIT
# and the error/warning event names are kept verbatim on UspRadio.fpp.
radio = "ReferenceDeployment.uspRadio"

RADIO_ERROR_EVENTS = ("SendFailed", "ConfigurationFailed", "AllocationFailed")


@pytest.fixture(autouse=True)
def setup_test(fprime_test_api: IntegrationTestAPI, start_gds):
    """Fixture to set the downlink divider before each test and disable transmit after each test"""
    proves_send_and_assert_command(
        fprime_test_api,
        f"{downlinkDelay}.DIVIDER_PRM_SET",
        [20],
    )
    yield
    proves_send_and_assert_command(
        fprime_test_api,
        f"{radio}.TRANSMIT",
        ["DISABLED"],
    )


def test_01_transmit_enabled(fprime_test_api: IntegrationTestAPI, start_gds):
    """Enabling transmit must not produce any radio error/warning events."""
    start: TimeType = TimeType().set_datetime(
        datetime.now(), time_base=TimeType.TimeBase("TB_DONT_CARE")
    )

    proves_send_and_assert_command(
        fprime_test_api,
        f"{radio}.TRANSMIT",
        ["ENABLED"],
    )

    time.sleep(10)

    for evt in RADIO_ERROR_EVENTS:
        result = fprime_test_api.await_event(f"{radio}.{evt}", start=start, timeout=0)
        assert result is None, f"Unexpected {radio}.{evt}: {result}"

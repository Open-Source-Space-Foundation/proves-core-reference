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

pytestmark = pytest.mark.rf_unsafe

downlinkDelay = "ReferenceDeployment.downlinkDelay"
lora = "ReferenceDeployment.lora"

LORA_ERROR_EVENTS = ("SendFailed", "ConfigurationFailed", "AllocationFailed")


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
        f"{lora}.TRANSMIT",
        ["DISABLED"],
    )


def test_01_transmit_enabled(fprime_test_api: IntegrationTestAPI, start_gds):
    """Enabling transmit must not produce any LoRa error/warning events."""
    start: TimeType = TimeType().set_datetime(
        datetime.now(), time_base=TimeType.TimeBase("TB_DONT_CARE")
    )

    proves_send_and_assert_command(
        fprime_test_api,
        f"{lora}.TRANSMIT",
        ["ENABLED"],
    )

    time.sleep(10)

    for evt in LORA_ERROR_EVENTS:
        result = fprime_test_api.await_event(f"{lora}.{evt}", start=start, timeout=0)
        assert result is None, f"Unexpected {lora}.{evt}: {result}"

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

downlinkDelay = "ReferenceDeployment.downlinkDelay"
lora = "ReferenceDeployment.lora"

LORA_ERROR_EVENTS = ("SendFailed", "ConfigurationFailed", "AllocationFailed")


@pytest.fixture(autouse=True)
def setup_test(fprime_test_api: IntegrationTestAPI, start_gds, request):
    """Fixture to set the downlink divider before each test and disable transmit after each test"""
    proves_send_and_assert_command(
        fprime_test_api,
        f"{downlinkDelay}.DIVIDER_PRM_SET",
        [20],
    )
    yield
    if request.node.name != "test_00_setup_only":
        proves_send_and_assert_command(
            fprime_test_api,
            f"{lora}.TRANSMIT",
            ["DISABLED"],
        )


def test_00_setup_only(fprime_test_api: IntegrationTestAPI, start_gds):
    """Enable LoRa transmit; used as a standalone re-enable step before the RF integration pass."""
    proves_send_and_assert_command(
        fprime_test_api,
        f"{lora}.TRANSMIT",
        ["ENABLED"],
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

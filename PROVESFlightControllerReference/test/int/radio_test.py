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
lora = "ReferenceDeployment.lora"

LORA_ERROR_EVENTS = ("SendFailed", "ConfigurationFailed", "AllocationFailed")

# Continuous-wave burst duration (seconds) for the CW regression test. Kept short so
# the command stays within the GDS command-completion timeout while still exercising
# the asynchronous TxTimeout teardown path.
CW_SECONDS = 5


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


def test_02_continuous_wave_repeated(fprime_test_api: IntegrationTestAPI, start_gds):
    """Regression test for issue #207.

    CONTINUOUS_WAVE used to return EXECUTION_ERROR on every call and physically
    transmit only on the first call, leaving the modem wedged (no TX/RX) until a
    board reset. The root cause was the handler re-arming RX while the asynchronous
    continuous wave still held the modem BUSY. This test sends CONTINUOUS_WAVE twice
    and asserts that both calls succeed, that no ConfigurationFailed event is logged
    by the second call, and that a normal transmit path still works afterwards (i.e.
    the modem is not wedged).
    """
    # First continuous-wave burst. Before the fix this still transmitted once but
    # returned EXECUTION_ERROR, so send_and_assert_command would already fail here.
    proves_send_and_assert_command(
        fprime_test_api,
        f"{lora}.CONTINUOUS_WAVE",
        [CW_SECONDS],
    )

    # Wait out the CW duration so the modem fully tears down before the next call.
    time.sleep(CW_SECONDS + 2)

    # Second continuous-wave burst. This is the call that failed before the fix
    # (modem left wedged after the first CW), so OK here is the key regression signal.
    start: TimeType = TimeType().set_datetime(
        datetime.now(), time_base=TimeType.TimeBase("TB_DONT_CARE")
    )
    proves_send_and_assert_command(
        fprime_test_api,
        f"{lora}.CONTINUOUS_WAVE",
        [CW_SECONDS],
    )
    time.sleep(CW_SECONDS + 2)

    # The repeated CW must not have logged a modem configuration failure.
    result = fprime_test_api.await_event(
        f"{lora}.ConfigurationFailed", start=start, timeout=0
    )
    assert result is None, f"Unexpected {lora}.ConfigurationFailed after repeated CW: {result}"

    # The radio must still be usable for normal transmission after the CW bursts;
    # a wedged modem would surface as LoRa error events here.
    start = TimeType().set_datetime(
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
        assert result is None, f"Unexpected {lora}.{evt} after repeated CW: {result}"

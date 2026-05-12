"""
1_lora_passthrough_test.py:

Smoke test for the downlink LoRa radio path via the v5d CircuitPython
passthrough board (code-lora.py). Runs right after 0_radio_test so the
flight controller's TRANSMIT is already on and the downlink divider is
already set; we still re-issue TRANSMIT ENABLED so the test passes when
run in isolation. Reads raw bytes from the passthrough's USB CDC data
interface and asserts non-trivial binary traffic arrives within a
fixed window. Skipped when LORA_PASSTHROUGH_TTY is unset.
"""

import os
import time

import pytest
import serial
from common import proves_send_and_assert_command
from fprime_gds.common.testing_fw.api import IntegrationTestAPI

pytestmark = pytest.mark.rf_unsafe

lora = "ReferenceDeployment.lora"
downlinkDelay = "ReferenceDeployment.downlinkDelay"
PASSTHROUGH_TTY = os.environ.get("LORA_PASSTHROUGH_TTY")
READ_WINDOW_S = 20.0
SETTLE_S = 2.0


@pytest.mark.skipif(not PASSTHROUGH_TTY, reason="LORA_PASSTHROUGH_TTY not set")
def test_lora_downlink_reaches_passthrough(
    fprime_test_api: IntegrationTestAPI, start_gds
):
    """Bytes flow through the v5d LoRa passthrough after TRANSMIT is enabled."""
    proves_send_and_assert_command(
        fprime_test_api, f"{downlinkDelay}.DIVIDER_PRM_SET", [20]
    )
    proves_send_and_assert_command(fprime_test_api, f"{lora}.TRANSMIT", ["ENABLED"])
    time.sleep(SETTLE_S)

    with serial.Serial(PASSTHROUGH_TTY, baudrate=115200, timeout=1.0) as ser:
        ser.reset_input_buffer()
        deadline = time.monotonic() + READ_WINDOW_S
        rx = bytearray()
        while time.monotonic() < deadline:
            chunk = ser.read(256)
            if chunk:
                rx.extend(chunk)

    assert len(rx) > 0, (
        f"No bytes received from {PASSTHROUGH_TTY} after enabling LoRa transmit"
    )

    binary_bytes = sum(1 for b in rx if b < 32 and b not in (9, 10, 13))
    assert binary_bytes > 0, (
        f"Only ASCII text received from {PASSTHROUGH_TTY}: {bytes(rx[:200])!r}. "
        "Likely opened the CircuitPython REPL CDC instead of the data CDC."
    )

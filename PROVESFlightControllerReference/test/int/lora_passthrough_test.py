"""
lora_passthrough_test.py:

Smoke test for the downlink LoRa radio via the CircuitPython passthrough
board (code-lora.py). Reads raw bytes from the passthrough's USB CDC data
interface and asserts non-trivial binary traffic arrives within a
fixed window.
"""

import os
import time

import pytest
import serial
from common import proves_send_and_assert_command
from fprime_gds.common.testing_fw.api import IntegrationTestAPI

lora = "ReferenceDeployment.lora"
downlinkDelay = "ReferenceDeployment.downlinkDelay"
PASSTHROUGH_TTY = os.environ.get("LORA_PASSTHROUGH_TTY")
READ_WINDOW_S = 20.0
SETTLE_S = 2.0


@pytest.mark.skip(
    reason="This test is only needed to debug new CI systems. Re-enable when needed."
)
def test_lora_downlink_reaches_passthrough(
    fprime_test_api: IntegrationTestAPI, start_gds
):
    """Bytes flow through the LoRa passthrough board after TRANSMIT is enabled."""
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

    control_bytes = sum(1 for b in rx if b < 32 and b not in (9, 10, 13))
    assert control_bytes > 0, (
        f"Only ASCII text received from {PASSTHROUGH_TTY}: {bytes(rx[:200])!r}. "
        "Likely opened the CircuitPython REPL CDC instead of the data CDC."
    )

"""
security_active_keys_test.py:

Exercises GET_ACTIVE_KEYS on TcSecurityDeframer: an operator who has lost track
of what was provisioned can query the active SPI(s) and a non-reversible
fingerprint of each key, without ever learning the key itself. Runs after
provision_key_test (alphabetically: "security" sorts after "provision_key"),
so the store is guaranteed to hold at least the CI key at SPI 0.
"""

import os
import re

import pytest
from common import proves_send_and_assert_command
from fprime_gds.common.data_types.event_data import EventData
from fprime_gds.common.testing_fw.api import IntegrationTestAPI

# 4-byte truncated SHA-256 fingerprint, hex-encoded: 8 lowercase hex characters
_FINGERPRINT_RE = re.compile(r"^[0-9a-f]{8}$")


def _deframer_for(request: pytest.FixtureRequest) -> str:
    link = request.config.getoption("--sync-deframer", default=None)
    if link is None:
        link = (
            "lora"
            if request.config.getoption("--with-radio", default=False)
            else "uart"
        )
    return {
        "uart": "ComCcsdsUart.tcSecurityDeframer",
        "lora": "ComCcsdsLora.tcSecurityDeframer",
    }[link]


def test_get_active_keys_reports_provisioned_spi(
    fprime_test_api: IntegrationTestAPI, start_gds, request: pytest.FixtureRequest
):
    """GET_ACTIVE_KEYS reports SPI=0 (provisioned by provision_key_test) with a fingerprint,
    and never echoes the raw key bytes."""
    deframer = _deframer_for(request)

    proves_send_and_assert_command(fprime_test_api, f"{deframer}.GET_ACTIVE_KEYS")

    evt: EventData = fprime_test_api.assert_event(
        f"{deframer}.ActiveKeyInfo", timeout=5
    )
    spi = evt.args[0].val
    fingerprint = str(evt.args[1].val)

    assert spi == 0, f"Expected SPI=0 (provisioned by provision_key_test), got {spi}"
    assert _FINGERPRINT_RE.match(fingerprint), (
        f"Fingerprint {fingerprint!r} is not an 8-character lowercase hex string"
    )

    key = os.environ.get("PROVES_AUTH_KEY")
    if key:
        assert fingerprint.lower() not in key.lower(), (
            "Fingerprint must never contain the raw key material"
        )
        assert key.lower() not in fingerprint.lower()

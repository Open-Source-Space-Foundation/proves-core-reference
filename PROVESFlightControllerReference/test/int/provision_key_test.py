"""
provision_key_test.py:

This module provisions the HMAC authentication key onto a keyless satellite
so ground and flight share a key. Must run before any auth-required command
is sent, on the bypass-allowlisted link (PROVISION_KEY is refused once the
on-flash key store already holds a key). Idempotent: if the board was
already provisioned by a previous run (the key store lives on internal
flash and survives reflashing), a NotEmpty rejection is treated as success
rather than a failure, since the store already holds the CI secret key.
"""

import os

import pytest
from fprime_gds.common.data_types.event_data import EventData
from fprime_gds.common.testing_fw.api import IntegrationTestAPI
from fprime_gds.common.testing_fw.predicates import is_a_member_of


@pytest.mark.provision_key
def test_provision_key(
    fprime_test_api: IntegrationTestAPI, start_gds, request: pytest.FixtureRequest
):
    """Provision the HMAC key (spi=0) on a keyless board, tolerating a prior provision"""
    link = request.config.getoption("--sync-deframer", default=None)
    if link is None:
        link = (
            "lora"
            if request.config.getoption("--with-radio", default=False)
            else "uart"
        )
    deframer = {
        "uart": "ComCcsdsUart.tcSecurityDeframer",
        "lora": "ComCcsdsLora.tcSecurityDeframer",
    }[link]

    key = os.environ.get("PROVES_AUTH_KEY")
    if not key:
        pytest.fail(
            "PROVES_AUTH_KEY environment variable not set; cannot provision key"
        )

    # await_event coerces any non-event_predicate into an *id* predicate, so the
    # "either outcome" match has to be expressed over event IDs. Passing a list of
    # event_predicates here would silently never match (they'd be evaluated against
    # an int).
    outcome_ids = [
        fprime_test_api.translate_event_name(f"{deframer}.KeyProvisioned"),
        fprime_test_api.translate_event_name(f"{deframer}.KeyProvisionFailed"),
    ]

    fprime_test_api.clear_histories()
    fprime_test_api.send_command(f"{deframer}.PROVISION_KEY", ["0", key])

    evt: EventData = fprime_test_api.await_event(
        is_a_member_of(outcome_ids),
        timeout=10,
    )

    assert evt is not None, (
        f"No KeyProvisioned/KeyProvisionFailed event from {deframer} within 10s of "
        "PROVISION_KEY; the command may not have reached the board"
    )

    if evt.template.get_full_name().endswith("KeyProvisionFailed"):
        status = evt.args[0].val
        assert status == "NotEmpty", (
            f"PROVISION_KEY failed with unexpected status {status!r}; "
            "board should either be keyless or already hold the CI key"
        )

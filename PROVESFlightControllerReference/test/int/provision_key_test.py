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
import random
import time

import pytest
from common import FIB_BACKOFF
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

    # Every authenticated test downstream depends on this one, so a single dropped uplink must not
    # fail the whole provisioning gate. proves_send_and_assert_command can't be used here (the
    # outcome is an either/or event pair, and a command response alone doesn't tell us which), so
    # retry by hand on the same Fibonacci backoff it uses to absorb LoRa's half-duplex collisions.
    evt: EventData | None = None
    for attempt in range(len(FIB_BACKOFF)):
        fprime_test_api.clear_histories()
        fprime_test_api.send_command(f"{deframer}.PROVISION_KEY", ["0", key])

        evt = fprime_test_api.await_event(
            is_a_member_of(outcome_ids),
            timeout=10,
        )
        if evt is not None:
            break

        time.sleep(FIB_BACKOFF[attempt] * random.uniform(0.5, 1.5))

    assert evt is not None, (
        f"No KeyProvisioned/KeyProvisionFailed event from {deframer} after "
        f"{len(FIB_BACKOFF)} PROVISION_KEY attempts; the command may not be reaching the board"
    )

    if evt.template.get_full_name().endswith("KeyProvisionFailed"):
        status = evt.args[0].val
        # NotEmpty is the ONLY tolerated failure, and only because CI boards keep the key store on
        # a littlefs partition that survives reflashing, so every run after the first re-provisions
        # a board that is already provisioned.
        #
        # StoreUnreadable in particular must never be tolerated here. On a board that already holds
        # a key it means the store went unreadable (a real fault); on a keyless board it is the
        # signature of the cold-provisioning lockout - the board cannot be commanded at all, and
        # letting this pass is precisely how that shipped. See test_cold_provision_gap below.
        assert status == "NotEmpty", (
            f"PROVISION_KEY failed with unexpected status {status!r}; "
            "board should either be keyless or already hold the CI key"
        )


@pytest.mark.provision_key
def test_cold_provision_gap():
    """Placeholder for the cold-provision path, which is not yet reachable from CI.

    The bug this file's assertions now guard against (PROVISION_KEY refused with StoreUnreadable on
    a board whose key store file does not exist) can only be exercised against a *genuinely blank*
    key store. Nothing in the test fleet can produce that state:

      * every bench/CI board was provisioned under an earlier firmware revision, and the littlefs
        keystore_partition survives reflashing, so the store is never absent;
      * the existing ``fsFormat.FORMAT`` command formats the FatFS root ``/``, NOT the littlefs
        ``/keys`` partition, so it cannot clear the store either.

    Automating this therefore needs new firmware capability - either a command that erases the
    keystore partition (which must itself be authenticated, since it is a remote-bricking primitive
    if it is not) or a CI step that flashes a blank keystore partition image over SWD. Rather than
    fake a cold board with a mock that would re-hide the host/target divergence, the invariant is
    covered exhaustively at the unit level in
    ``test/unit-tests/test_TcSecurityDeframer_KeyStorePolicy.cpp``, and this test is left as an
    explicit, visible gap.

    TODO(#472): implement once /keys can be erased or pre-flashed blank from CI.
    """
    pytest.skip(
        "Cold-provision path needs a blank /keys partition; no mechanism exists yet to produce "
        "one from CI (fsFormat.FORMAT targets FatFS /, not littlefs /keys). Covered at unit level "
        "by test_TcSecurityDeframer_KeyStorePolicy.cpp."
    )

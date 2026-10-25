"""
common.py:

This module provides a functions and constants shared by
integration tests for PROVES microcontroller hardware.
"""

import os
import random
import time
from collections.abc import Callable, Iterable
from functools import reduce

from fprime_gds.common.testing_fw.api import IntegrationTestAPI
from fprime_gds.common.testing_fw.predicates import event_predicate

# Fibonacci backoff base values (seconds) indexed by attempt number (0-based).
# Used by proves_send_and_assert_command to spread retries across time and
# reduce half-duplex collisions on the LoRa radio link.  Exported so that
# test modules with their own outer retry loops can reference the same sequence.
FIB_BACKOFF = [1, 1, 2, 3, 5, 8, 13]

cmdDispatch = "CdhCore.cmdDisp"

# Raised to 5 by conftest when --with-radio is set; radio link is slower and
# lossier than UART so the wider retry budget absorbs transient packet drops.
_DEFAULT_RETRIES = 3

# After this many consecutive command failures the radio link is assumed to be
# in a bad state (e.g. TRANSMIT toggled off mid-sequence) and the registered
# recovery callback is invoked to re-enable TRANSMIT before the next retry.
RADIO_RECOVER_THRESHOLD = 3

# Callable registered by conftest when --with-radio is active.  Invoked inside
# proves_send_and_assert_command every RADIO_RECOVER_THRESHOLD failures to
# re-send TRANSMIT ENABLED and restore the RF downlink.
_radio_recover_fn: Callable[[], None] | None = None


def set_default_retries(n: int) -> None:
    global _DEFAULT_RETRIES
    _DEFAULT_RETRIES = n


def set_radio_recover_fn(fn: Callable[[], None] | None) -> None:
    """Register a callable to re-establish the radio link.

    When ``--with-radio`` is active, ``conftest`` registers a function that
    re-sends TRANSMIT ENABLED.  ``proves_send_and_assert_command`` calls it
    after every ``RADIO_RECOVER_THRESHOLD`` consecutive failures so that a
    stale or temporarily-disabled radio link does not permanently block command
    delivery over the rest of the test run.
    """
    global _radio_recover_fn
    _radio_recover_fn = fn


# The framer plugin's file holds the next sequence number it sends, and the plugin
# reads it on every frame (Framing/src/authenticate_plugin.py). The plugin honors
# PROVES_SEQUENCE_NUMBER_FILE, so the tests do too.
SEQUENCE_NUMBER_FILE = (
    os.environ.get("PROVES_SEQUENCE_NUMBER_FILE") or "./Framing/src/sequence_number.bin"
)


def write_next_sequence_number(board_seq: int) -> None:
    """Make ground's next frame carry the number after the board's last accepted one.

    Wrapped to the U32 sequence field. Written to a sibling file and renamed
    over the original, so the framer never reads a truncated file.
    """
    temporary = f"{SEQUENCE_NUMBER_FILE}.tmp"
    with open(temporary, "w", encoding="utf-8") as f:
        f.write(str((board_seq + 1) % 2**32))
    os.replace(temporary, SEQUENCE_NUMBER_FILE)


def sequence_number_at_or_after(a: int, b: int) -> bool:
    """Return whether U32 sequence number `a` equals `b` or is ahead of it.

    Serial number arithmetic (RFC 1982), as the deframer's validator uses, so
    the order holds across the 2**32 wrap: 1 is ahead of 0xFFFFFFFF.
    """
    return (a - b) % 2**32 < 2**31


def latest_sequence_number(numbers: Iterable[int]) -> int:
    """Return the number in `numbers` that is furthest ahead, wrap-aware."""
    return reduce(
        lambda latest, n: n if sequence_number_at_or_after(n, latest) else latest,
        numbers,
    )


def resync_sequence_number(
    fprime_test_api: IntegrationTestAPI,
    deframer: str = "ComCcsdsUart.tcSecurityDeframer",
) -> None:
    """Fast-forward the framer plugin's sequence file if the board's counter is ahead.

    After a reboot the board can expect a higher sequence number than ground
    holds and rejects every authenticated command until ground catches up.
    """
    fprime_test_api.clear_histories()
    fprime_test_api.send_command(f"{deframer}.GET_SEQ_NUM")
    evt = fprime_test_api.await_event(f"{deframer}.SequenceNumberGet", timeout=5)
    if evt is None:
        raise AssertionError(f"no {deframer}.SequenceNumberGet event within 5 seconds")
    board_seq = int(evt.args[0].val)
    try:
        with open(SEQUENCE_NUMBER_FILE, "r", encoding="utf-8") as f:
            ground_next = int(f.read().strip() or 0)
    except (OSError, ValueError):
        # No usable ground number to keep: take the board's.
        write_next_sequence_number(board_seq)
        return
    # Ground's next number is a replay unless it is past the board's last accepted.
    if sequence_number_at_or_after(board_seq, ground_next):
        write_next_sequence_number(board_seq)


def proves_send_and_assert_command(
    fprime_test_api: IntegrationTestAPI,
    command: str,
    args: list[str] = [],
    events: list[event_predicate] = [],
    retries: int | None = None,
):
    """Send command and assert completion

    PROVES microcontroller hardware responds more slowly than typical FPrime
    hardware which use microprocessors. As a result, some commands may
    take longer to complete. This function clears histories before sending
    the command, sets a longer timeout for command completion, and retries
    up to `retries` times if command assertion fails (default: module-level
    _DEFAULT_RETRIES, bumped to 5 for radio runs via --with-radio).
    """
    attempts = retries if retries is not None else _DEFAULT_RETRIES
    for attempt in range(attempts):
        fprime_test_api.clear_histories()
        try:
            fprime_test_api.send_and_assert_command(
                command,
                args,
                timeout=10,
                max_delay=10,
                events=[],
            )
            if events:
                fprime_test_api.assert_event_sequence(events, timeout=5)
            break
        except AssertionError:
            if attempt == attempts - 1:
                raise
            # After RADIO_RECOVER_THRESHOLD consecutive failures, re-enable
            # TRANSMIT before continuing to retry.  Rapid test-to-test state
            # transitions can leave the radio link in a dormant state — rapid
            # successive enable/disable cycles are not always reflected before
            # the next test's first command attempt.  Calling the recovery
            # function (registered by conftest when --with-radio is active)
            # re-sends TRANSMIT ENABLED so the link is live for the next try.
            if (
                _radio_recover_fn is not None
                and (attempt + 1) % RADIO_RECOVER_THRESHOLD == 0
            ):
                _radio_recover_fn()
            # Fibonacci backoff with ±50% jitter before the next retry.
            # The LoRa radio link is half-duplex: the satellite cannot receive
            # an uplink command while it is transmitting events/telemetry
            # downlink.  Retrying immediately can repeatedly collide with the
            # same downlink burst.  Spreading retries across Fibonacci-scaled
            # intervals with random jitter reduces collision probability.
            base = FIB_BACKOFF[min(attempt, len(FIB_BACKOFF) - 1)]
            delay = base * random.uniform(0.5, 1.5)
            time.sleep(delay)

"""
common.py:

This module provides a functions and constants shared by
integration tests for PROVES microcontroller hardware.
"""

from fprime_gds.common.testing_fw.api import IntegrationTestAPI
from fprime_gds.common.testing_fw.predicates import event_predicate

cmdDispatch = "CdhCore.cmdDisp"

# Raised to 5 by conftest when --with-radio is set; radio link is slower and
# lossier than UART so the wider retry budget absorbs transient packet drops.
_DEFAULT_RETRIES = 3


def set_default_retries(n: int) -> None:
    global _DEFAULT_RETRIES
    _DEFAULT_RETRIES = n


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

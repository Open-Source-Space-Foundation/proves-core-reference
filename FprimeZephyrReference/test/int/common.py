"""
common.py:

This module provides a functions and constants shared by
integration tests for PROVES microcontroller hardware.
"""

from fprime_gds.common.testing_fw.api import IntegrationTestAPI
from fprime_gds.common.testing_fw.predicates import event_predicate

cmdDispatch = "CdhCore.cmdDisp"


def proves_send_and_assert_command(
    fprime_test_api: IntegrationTestAPI,
    command: str,
    args: list[str] = [],
    events: list[event_predicate] = [],
):
    """Send command and assert completion

    PROVES microcontroller hardware responds more slowly than typical FPrime
    hardware which use microprocessors. As a result, some commands may
    take longer to complete. This function clears histories before sending
    the command, sets a longer timeout for command completion, and retries
    up to 3 times if command assertion fails.
    """
    for attempt in range(3):
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
            if attempt == 2:
                raise

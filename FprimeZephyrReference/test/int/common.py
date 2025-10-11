"""
common.py:

This module provides a helper functions for sending commands and asserting their completion
in integration tests for PROVES microcontroller hardware, accommodating slower response times.
"""

from fprime_gds.common.testing_fw.api import IntegrationTestAPI


def proves_send_and_assert_command(
    fprime_test_api: IntegrationTestAPI, command: str, args: list[str] = []
):
    """Send command and assert completion

    PROVES microcontroller hardware responds more slowly than typical FPrime
    hardware which use microprocessors. As a result, some commands may
    take longer to complete. This function clears histories before sending
    the command, sets a longer timeout for command completion, and retries
    up to 3 times if command assertion fails.
    """
    fprime_test_api.clear_histories()

    for attempt in range(3):
        try:
            fprime_test_api.send_and_assert_command(
                command,
                args,
                max_delay=10,
            )
            break
        except AssertionError:
            if attempt == 2:
                raise

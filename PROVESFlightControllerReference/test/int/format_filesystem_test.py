"""
format_filesystem_test.py:

This test runs the format command to reset the filesystem if it has gotten into a bad state. You may need to manually restart the satellite after running this command, so use caution.
"""

import pytest
from fprime_gds.common.testing_fw.api import IntegrationTestAPI


@pytest.mark.format_filesystem
def test_format_filesystem(fprime_test_api: IntegrationTestAPI, start_gds):
    """Format the filesystem and wait for the board to report the result"""
    # One attempt with a long timeout: a retry would format again, and the job
    # resets the board next, so returning before mkfs finishes can corrupt the card.
    fprime_test_api.send_and_assert_command(
        "ReferenceDeployment.fsFormat.FORMAT", timeout=60
    )

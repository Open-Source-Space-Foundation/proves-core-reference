"""
format_filesystem_test.py:

This test runs the format command to reset the filesystem if it has gotten into a bad state. You may need to manually restart the satellite after running this command, so use caution.
"""

import pytest
from common import proves_send_and_assert_command
from fprime_gds.common.testing_fw.api import IntegrationTestAPI


@pytest.mark.format_filesystem
def test_format_filesystem(fprime_test_api: IntegrationTestAPI, start_gds):
    """Send command to format the filesystem"""
    proves_send_and_assert_command(
        fprime_test_api, "ReferenceDeployment.fsFormat.FORMAT"
    )

"""
format_filesystem_test.py:

This test runs the format command to reset the filesystem if it has gotten into a bad state. You may need to manually restart the satellite after running this command, so use caution.
"""

import pytest
from fprime_gds.common.testing_fw.api import IntegrationTestAPI


@pytest.mark.format_filesystem
def test_format_filesystem(fprime_test_api: IntegrationTestAPI, start_gds):
    """Send command to format the filesystem"""
    fprime_test_api.send_command("ReferenceDeployment.fsFormat.FORMAT")

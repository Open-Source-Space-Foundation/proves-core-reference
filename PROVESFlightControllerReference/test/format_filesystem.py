"""
format_filesystem.py:

This test runs the format command to reset the filesystem if it has gotten into a bad state. You may need to manually restart the satellite after running this command, so use caution.
"""

import time

import pytest
from fprime_gds.common.testing_fw.api import IntegrationTestAPI
from int.common import cmdDispatch, proves_send_and_assert_command


@pytest.fixture(scope="session", autouse=True)
def check_gds(fprime_test_api_session: IntegrationTestAPI):
    """Fixture to check GDS is running"""

    gds_working = False
    timeout_time = time.time() + 30
    while time.time() < timeout_time:
        try:
            fprime_test_api_session.send_and_assert_command(
                command=f"{cmdDispatch}.CMD_NO_OP"
            )
            gds_working = True
            break
        except Exception:
            time.sleep(1)
    assert gds_working

    return


def test_format_filesystem(fprime_test_api: IntegrationTestAPI):
    """Send command to format the filesystem"""
    proves_send_and_assert_command(
        fprime_test_api, "ReferenceDeployment.fsFormat.FORMAT"
    )

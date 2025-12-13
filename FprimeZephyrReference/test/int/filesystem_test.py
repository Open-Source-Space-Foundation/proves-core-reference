"""
filesystem_test.py:

Integration tests for the filesystem.
"""

from datetime import datetime

from common import proves_send_and_assert_command
from fprime.common.models.serialize.time_type import TimeType
from fprime_gds.common.testing_fw.api import IntegrationTestAPI

fileManager = "FileHandling.fileManager"


def test_01_get_temperature(fprime_test_api: IntegrationTestAPI, start_gds):
    """Test that we can get list the root directory"""
    start: TimeType = TimeType().set_datetime(datetime.now())
    proves_send_and_assert_command(
        fprime_test_api,
        f"{fileManager}.ListDirectory",
        ["//"],
    )
    fprime_test_api.assert_event(
        f"{fileManager}.ListDirectorySucceeded", start=start, timeout=2
    )

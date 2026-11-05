"""
boot_count_test.py:

Checks the boot count against --expect-boot-count. sim/smoke.sh runs it across
a power cut and a warm reset on the native_sim board.
"""

import pytest
from common import proves_send_and_assert_command
from fprime_gds.common.testing_fw.api import IntegrationTestAPI

startup_manager = "ReferenceDeployment.startupManager"


def test_boot_count(
    fprime_test_api: IntegrationTestAPI, start_gds, request: pytest.FixtureRequest
):
    """The boot count equals --expect-boot-count"""
    expected = request.config.getoption("--expect-boot-count")
    if expected is None:
        pytest.skip("Needs --expect-boot-count")

    fprime_test_api.clear_histories()
    proves_send_and_assert_command(fprime_test_api, f"{startup_manager}.GET_BOOT_COUNT")
    result = fprime_test_api.assert_event(
        f"{startup_manager}.CurrentBootCount", timeout=3
    )
    assert result.args[0].val == expected

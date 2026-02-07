"""
reset_manager_test.py:

Integration tests for the Reset Manager component.
"""

from datetime import datetime

import pytest
from fprime_gds.common.models.serialize.time_type import TimeType
from fprime_gds.common.testing_fw.api import IntegrationTestAPI

resetManager = "ReferenceDeployment.resetManager"


def test_01_cold_reset(fprime_test_api: IntegrationTestAPI, start_gds):
    """Test that we can initiate a cold reset"""
    start: TimeType = TimeType().set_datetime(
        datetime.now(), time_base=TimeType.TimeBase("TB_DONT_CARE")
    )

    fprime_test_api.send_command(f"{resetManager}.COLD_RESET")

    # Assert FPrime restarted by checking for FrameworkVersion event
    fprime_test_api.assert_event(
        "CdhCore.version.FrameworkVersion", start=start, timeout=15
    )


@pytest.mark.skip(
    reason="Pre-existing bug: warm reset crashes system, preventing subsequent tests from running"
)
def test_02_warm_reset(fprime_test_api: IntegrationTestAPI, start_gds):
    """Test that we can initiate a warm reset"""
    start: TimeType = TimeType().set_datetime(
        datetime.now(), time_base=TimeType.TimeBase("TB_DONT_CARE")
    )

    fprime_test_api.send_command(f"{resetManager}.WARM_RESET")

    # Assert FPrime restarted by checking for FrameworkVersion event
    fprime_test_api.assert_event(
        "CdhCore.version.FrameworkVersion", start=start, timeout=15
    )


def test_03_radio_reset(fprime_test_api: IntegrationTestAPI, start_gds):
    """Test that we can initiate a radio reset without system reboot"""

    # Clear event history before test
    fprime_test_api.clear_histories()

    # Send RESET_RADIO command
    fprime_test_api.send_and_assert_command(f"{resetManager}.RESET_RADIO", timeout=5)

    # Assert INITIATE_RADIO_RESET event was emitted
    fprime_test_api.assert_event(f"{resetManager}.INITIATE_RADIO_RESET", timeout=2)

    # Verify system is still running (not rebooted like cold/warm reset)
    # We can do this by sending another simple command
    fprime_test_api.send_and_assert_command("CdhCore.cmdDisp.CMD_NO_OP", timeout=2)

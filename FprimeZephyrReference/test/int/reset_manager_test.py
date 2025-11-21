"""
reset_manager_test.py:

Integration tests for the Reset Manager component.
"""

from fprime_gds.common.testing_fw.api import IntegrationTestAPI

resetManager = "ReferenceDeployment.resetManager"


def test_01_cold_reset(fprime_test_api: IntegrationTestAPI, start_gds):
    """Test that we can initiate a cold reset"""

    fprime_test_api.send_command(f"{resetManager}.COLD_RESET")

    # Assert FPrime restarted by checking for FrameworkVersion event
    fprime_test_api.assert_event("CdhCore.version.FrameworkVersion", timeout=10)


def test_02_warm_reset(fprime_test_api: IntegrationTestAPI, start_gds):
    """Test that we can initiate a warm reset"""

    fprime_test_api.send_command(f"{resetManager}.WARM_RESET")

    # Assert FPrime restarted by checking for FrameworkVersion event
    fprime_test_api.assert_event("CdhCore.version.FrameworkVersion", timeout=10)

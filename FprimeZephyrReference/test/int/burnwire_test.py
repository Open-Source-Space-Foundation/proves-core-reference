"""
watchdog_test.py:

Integration tests for the Watchdog component.
"""

from fprime_gds.common.testing_fw.api import IntegrationTestAPI


def test_01_start_timeout_watchdog(fprime_test_api: IntegrationTestAPI):
    fprime_test_api.send_and_assert_command(
        "ReferenceDeployment.burnwire.START_BURNWIRE", max_delay=2
    )

    # should be on
    fprime_test_api.assert_event(
        "ReferenceDeployment.burnwire.SetBurnwireState", timeout=2
    )

    # should be on
    fprime_test_api.assert_event(
        "ReferenceDeployment.burnwire.SafetyTimerStatus", timeout=30
    )
    yield
    # should be off
    fprime_test_api.assert_event(
        "ReferenceDeployment.burnwire.SafetyTimerStatus", timeout=30
    )
    # shoulf be off
    fprime_test_api.assert_event(
        "ReferenceDeployment.burnwire.SetBurnwireState", timeout=30
    )


def test_02_start_stop_watchdog(fprime_test_api: IntegrationTestAPI):
    fprime_test_api.send_and_assert_command(
        "ReferenceDeployment.burnwire.START_BURNWIRE", max_delay=2
    )

    # should be on
    fprime_test_api.assert_event(
        "ReferenceDeployment.burnwire.SetBurnwireState", timeout=2
    )

    # should be on
    fprime_test_api.assert_event(
        "ReferenceDeployment.burnwire.SafetyTimerStatus", timeout=30
    )

    fprime_test_api.send_and_assert_command(
        "ReferenceDeployment.burnwire.STOP_BURNWIRE", max_delay=2
    )

    # should be off
    fprime_test_api.assert_event(
        "ReferenceDeployment.burnwire.SetBurnwireState", timeout=2
    )

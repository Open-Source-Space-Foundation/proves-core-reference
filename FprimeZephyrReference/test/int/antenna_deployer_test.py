"""
antenna_deployer_test.py:

Integration tests for the Antenna Deployer component.
"""

import pytest
from common import proves_send_and_assert_command
from fprime_gds.common.data_types.event_data import EventData
from fprime_gds.common.testing_fw.api import IntegrationTestAPI

antenna_deployer = "ReferenceDeployment.antennaDeployer"
burnwire = "ReferenceDeployment.burnwire"


@pytest.fixture(autouse=True)
def configure_antenna_deployer(fprime_test_api: IntegrationTestAPI, start_gds):
    """Reduce deployment timing constants for tests and restore defaults afterwards"""
    overrides: list[tuple[str, int]] = [
        ("QUIET_TIME_SEC", 0),
        ("RETRY_DELAY_SEC", 0),
        ("BURN_DURATION_SEC", 1),
        ("MAX_DEPLOY_ATTEMPTS", 1),
    ]

    defaults: list[tuple[str, int]] = [
        ("QUIET_TIME_SEC", 120),
        ("RETRY_DELAY_SEC", 30),
        ("BURN_DURATION_SEC", 8),
        ("MAX_DEPLOY_ATTEMPTS", 3),
    ]

    # Ensure a clean starting point
    fprime_test_api.clear_histories()
    proves_send_and_assert_command(fprime_test_api, f"{burnwire}.STOP_BURNWIRE")

    for param, value in overrides:
        proves_send_and_assert_command(
            fprime_test_api, f"{antenna_deployer}.{param}_PRM_SET", [value]
        )

    yield

    # Stop the burnwire and restore defaults to avoid impacting other tests
    proves_send_and_assert_command(fprime_test_api, f"{burnwire}.STOP_BURNWIRE")
    for param, value in defaults:
        proves_send_and_assert_command(
            fprime_test_api, f"{antenna_deployer}.{param}_PRM_SET", [value]
        )

    fprime_test_api.clear_histories()


def test_deploy_without_distance_sensor(fprime_test_api: IntegrationTestAPI, start_gds):
    """Verify the antenna deployer drives the burnwire and reports failure without distance data"""

    proves_send_and_assert_command(fprime_test_api, f"{antenna_deployer}.DEPLOY")

    attempt_event: EventData = fprime_test_api.assert_event(
        f"{antenna_deployer}.DeployAttempt", timeout=5
    )
    assert attempt_event.args[0].val == 1, "First deployment attempt should be attempt #1"

    # Burnwire should be commanded on and then off after the shortened burn window
    fprime_test_api.assert_event(f"{burnwire}.SetBurnwireState", "ON", timeout=5)
    fprime_test_api.assert_event(f"{burnwire}.SetBurnwireState", "OFF", timeout=15)

    finish_event: EventData = fprime_test_api.assert_event(
        f"{antenna_deployer}.DeployFinish", timeout=10
    )
    # Result should be FAILED (enum value 2) after a single attempt
    assert (
        finish_event.args[0].val == "DEPLOY_RESULT_FAILED"
    ), "Deployment should fail without distance sensor feedback"
    assert finish_event.args[1].val == 1, "Exactly one attempt should be recorded"

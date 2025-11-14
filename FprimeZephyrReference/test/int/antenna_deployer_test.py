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
    # Reset deployment state to stop any in-progress deployment and reset counters
    proves_send_and_assert_command(
        fprime_test_api, f"{antenna_deployer}.SET_DEPLOYMENT_STATE", [False]
    )

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
    assert attempt_event.args[0].val == 1, (
        "First deployment attempt should be attempt #1"
    )

    # Burnwire should be commanded on and then off after the shortened burn window
    fprime_test_api.assert_event(f"{burnwire}.SetBurnwireState", "ON", timeout=5)
    fprime_test_api.assert_event(f"{burnwire}.SetBurnwireState", "OFF", timeout=15)

    finish_event: EventData = fprime_test_api.assert_event(
        f"{antenna_deployer}.DeployFinish", timeout=10
    )
    # Result should be FAILED (enum value 2) after a single attempt
    assert finish_event.args[0].val == "DEPLOY_RESULT_FAILED", (
        "Deployment should fail without distance sensor feedback"
    )
    assert finish_event.args[1].val == 1, "Exactly one attempt should be recorded"


def test_change_quiet_time_sec(fprime_test_api: IntegrationTestAPI, start_gds):
    """Changes the quiet_time_sec parameter and makes sure the antenna will wait for it to run"""

    # Set a specific quiet time (5 seconds) to test the parameter
    proves_send_and_assert_command(
        fprime_test_api, f"{antenna_deployer}.QUIET_TIME_SEC_PRM_SET", [5]
    )

    # Start deployment
    proves_send_and_assert_command(fprime_test_api, f"{antenna_deployer}.DEPLOY")

    # Wait for quiet time to expire and verify the elapsed time
    quiet_time_expired_event: EventData = fprime_test_api.assert_event(
        f"{antenna_deployer}.QuietTimeExpired", timeout=10
    )
    assert quiet_time_expired_event.args[0].val == 5, (
        "Quiet time should have elapsed for exactly 5 seconds"
    )

    # Verify deployment attempt starts after quiet time expires
    attempt_event: EventData = fprime_test_api.assert_event(
        f"{antenna_deployer}.DeployAttempt", timeout=2
    )
    assert attempt_event.args[0].val == 1, (
        "First deployment attempt should be attempt #1"
    )

    # Verify burnwire starts after quiet time
    fprime_test_api.assert_event(f"{burnwire}.SetBurnwireState", "ON", timeout=2)

    # Verify burnwire stops after burn duration
    fprime_test_api.assert_event(f"{burnwire}.SetBurnwireState", "OFF", timeout=15)

    # Verify deployment finishes with failure (no distance sensor)
    finish_event: EventData = fprime_test_api.assert_event(
        f"{antenna_deployer}.DeployFinish", timeout=10
    )
    assert finish_event.args[0].val == "DEPLOY_RESULT_FAILED"
    assert finish_event.args[1].val == 1


def test_multiple_deploy_attempts(fprime_test_api: IntegrationTestAPI, start_gds):
    """Changes the deploy attempts parameter and ensures the burnwire deploys multiple times"""

    # Set parameters after fixture has run to override fixture values
    proves_send_and_assert_command(
        fprime_test_api, f"{antenna_deployer}.MAX_DEPLOY_ATTEMPTS_PRM_SET", [3]
    )
    proves_send_and_assert_command(
        fprime_test_api, f"{antenna_deployer}.RETRY_DELAY_SEC_PRM_SET", [1]
    )
    proves_send_and_assert_command(
        fprime_test_api, f"{antenna_deployer}.QUIET_TIME_SEC_PRM_SET", [0]
    )
    proves_send_and_assert_command(
        fprime_test_api, f"{antenna_deployer}.BURN_DURATION_SEC_PRM_SET", [1]
    )

    # Start deployment
    proves_send_and_assert_command(fprime_test_api, f"{antenna_deployer}.DEPLOY")

    # Verify first attempt
    attempt_event: EventData = fprime_test_api.assert_event(
        f"{antenna_deployer}.DeployAttempt", timeout=5
    )
    assert attempt_event.args[0].val == 1, "First attempt should be #1"

    # Verify first burnwire cycle
    fprime_test_api.assert_event(f"{burnwire}.SetBurnwireState", "ON", timeout=2)
    fprime_test_api.assert_event(f"{burnwire}.SetBurnwireState", "OFF", timeout=15)

    # Clear histories to avoid finding the first attempt again
    fprime_test_api.clear_histories()

    # Wait for retry delay and verify second attempt
    attempt_event = fprime_test_api.assert_event(
        f"{antenna_deployer}.DeployAttempt", timeout=15
    )
    assert attempt_event.args[0].val == 2, "Second attempt should be #2"

    # Verify second burnwire cycle
    fprime_test_api.assert_event(f"{burnwire}.SetBurnwireState", "ON", timeout=2)
    fprime_test_api.assert_event(f"{burnwire}.SetBurnwireState", "OFF", timeout=15)

    # Clear histories to avoid finding previous attempts
    fprime_test_api.clear_histories()

    # Wait for retry delay and verify third attempt
    attempt_event = fprime_test_api.assert_event(
        f"{antenna_deployer}.DeployAttempt", timeout=15
    )
    assert attempt_event.args[0].val == 3, "Third attempt should be #3"

    # Verify third burnwire cycle
    fprime_test_api.assert_event(f"{burnwire}.SetBurnwireState", "ON", timeout=2)
    fprime_test_api.assert_event(f"{burnwire}.SetBurnwireState", "OFF", timeout=15)

    # Verify final failure after exhausting all attempts
    finish_event: EventData = fprime_test_api.assert_event(
        f"{antenna_deployer}.DeployFinish", timeout=15
    )
    assert finish_event.args[0].val == "DEPLOY_RESULT_FAILED"
    assert finish_event.args[1].val == 3, "Should have completed 3 attempts"


def test_burn_duration_sec(fprime_test_api: IntegrationTestAPI, start_gds):
    """Changes the burn duration sec parameter and ensures the burnwire burns for that long based on the burnwire events"""

    # Set parameters after fixture has run to override fixture values
    proves_send_and_assert_command(
        fprime_test_api, f"{antenna_deployer}.BURN_DURATION_SEC_PRM_SET", [3]
    )
    proves_send_and_assert_command(
        fprime_test_api, f"{antenna_deployer}.QUIET_TIME_SEC_PRM_SET", [0]
    )
    proves_send_and_assert_command(
        fprime_test_api, f"{antenna_deployer}.RETRY_DELAY_SEC_PRM_SET", [0]
    )
    proves_send_and_assert_command(
        fprime_test_api, f"{antenna_deployer}.MAX_DEPLOY_ATTEMPTS_PRM_SET", [1]
    )

    fprime_test_api.clear_histories()
    # Start deployment
    proves_send_and_assert_command(fprime_test_api, f"{antenna_deployer}.DEPLOY")

    # Wait for deployment attempt to start
    attempt_event: EventData = fprime_test_api.assert_event(
        f"{antenna_deployer}.DeployAttempt", timeout=5
    )
    assert attempt_event.args[0].val == 1, (
        "First deployment attempt should be attempt #1"
    )

    # Verify burnwire starts
    fprime_test_api.assert_event(f"{burnwire}.SetBurnwireState", "ON", timeout=2)

    # Wait for burnwire to stop
    fprime_test_api.assert_event(f"{burnwire}.SetBurnwireState", "OFF", timeout=15)

    # Verify the antenna deployer reports the burn duration in ticks (seconds)
    burn_signal_event: EventData = fprime_test_api.assert_event(
        f"{antenna_deployer}.AntennaBurnSignalCount", timeout=5
    )
    assert burn_signal_event.args[0].val == 3, (
        "Burn signal should have been active for 3 scheduler ticks"
    )

    # Verify deployment finishes with failure (no distance sensor)
    finish_event: EventData = fprime_test_api.assert_event(
        f"{antenna_deployer}.DeployFinish", timeout=10
    )
    assert finish_event.args[0].val == "DEPLOY_RESULT_FAILED"


def test_deployment_prevention_after_success(
    fprime_test_api: IntegrationTestAPI, start_gds
):
    """Verify that deployment is prevented once the deployed-state flag is asserted and can be reset"""

    # Ensure the persistent flag is cleared before starting
    proves_send_and_assert_command(
        fprime_test_api, f"{antenna_deployer}.SET_DEPLOYMENT_STATE", [False]
    )

    # Force the antenna to appear deployed without completing a deployment cycle
    proves_send_and_assert_command(
        fprime_test_api, f"{antenna_deployer}.SET_DEPLOYMENT_STATE", [True]
    )

    fprime_test_api.clear_histories()

    # Deployment should be skipped immediately
    proves_send_and_assert_command(fprime_test_api, f"{antenna_deployer}.DEPLOY")
    fprime_test_api.assert_event(
        f"{antenna_deployer}.DeploymentAlreadyComplete", timeout=2
    )
    # Verify that DeployAttempt event does NOT occur (assert_event will raise AssertionError if event is missing)
    with pytest.raises(AssertionError):
        fprime_test_api.assert_event(f"{antenna_deployer}.DeployAttempt", timeout=1)

    # Clear the flag and confirm deployments are permitted again
    proves_send_and_assert_command(
        fprime_test_api, f"{antenna_deployer}.SET_DEPLOYMENT_STATE", [False]
    )
    fprime_test_api.clear_histories()
    proves_send_and_assert_command(fprime_test_api, f"{antenna_deployer}.DEPLOY")
    fprime_test_api.assert_event(f"{antenna_deployer}.DeployAttempt", timeout=5)

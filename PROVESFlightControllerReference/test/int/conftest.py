"""
conftest.py:

Pytest configuration for integration tests.
"""

import time

import pytest
from common import cmdDispatch
from fprime_gds.common.testing_fw.api import IntegrationTestAPI


def pytest_addoption(parser: pytest.Parser) -> None:
    parser.addoption(
        "--with-radio",
        action="store_true",
        default=False,
        help="Enable radio setup and teardown fixtures for this test run.",
    )


@pytest.fixture(scope="session")
def start_gds(
    request: pytest.FixtureRequest, fprime_test_api_session: IntegrationTestAPI
):
    """Fixture to start GDS before tests and stop after tests

    GDS is used to send commands and receive telemetry/events.
    """
    gds_working = False
    timeout_time = time.time() + 30
    while time.time() < timeout_time:
        try:
            if request.config.getoption("--with-radio"):
                _enable_radio(fprime_test_api_session)
            fprime_test_api_session.send_and_assert_command(
                command=f"{cmdDispatch}.CMD_NO_OP"
            )
            gds_working = True
            break
        except Exception:
            time.sleep(1)
    assert gds_working

    yield


def _enable_radio(fprime_test_api: IntegrationTestAPI) -> None:
    # Divider must be set while TRANSMIT is DISABLED — radio parameter changes are latched on enable.
    # Setting it after enable leaves the default divider (300) active, producing a ~30s downlink gap
    # that exceeds the test sequence-search timeout.
    # Fire-and-forget (no per-call retry): start_gds wraps this in its own 30s
    # retry budget around a follow-up NO_OP liveness check. Wrapping each send
    # in proves_send_and_assert_command (3x10s) consumes that whole budget on
    # the first attempt and prevents start_gds from ever retrying.
    fprime_test_api.send_command(
        command="ReferenceDeployment.downlinkDelay.DIVIDER_PRM_SET",
        args=[20],
    )
    fprime_test_api.send_command(
        command="ReferenceDeployment.lora.TRANSMIT", args=["ENABLED"]
    )


@pytest.fixture(autouse=True)
def start_radio(request: pytest.FixtureRequest, fprime_test_api: IntegrationTestAPI):
    """Fixture to start the radio before tests"""
    if not request.config.getoption("--with-radio"):
        return

    _enable_radio(fprime_test_api)


@pytest.fixture(autouse=True)
def recover_from_safe_mode(
    request: pytest.FixtureRequest,
    fprime_test_api: IntegrationTestAPI,
    start_gds,
):
    """Best-effort: after each test, if FSW slipped into SAFE_MODE (e.g. low
    voltage brownout from a burnwire-heavy test, or a partial file upload
    triggering SafeModeSequenceFailed), send EXIT_SAFE_MODE so the next test
    doesn't inherit the broken state. Only runs in the radio pass — UART
    tests that exercise mode transitions handle their own cleanup.
    """
    yield
    if not request.config.getoption("--with-radio"):
        return
    try:
        fprime_test_api.clear_histories()
        fprime_test_api.send_and_assert_command(
            "ReferenceDeployment.modeManager.GET_CURRENT_MODE",
            timeout=5,
            max_delay=5,
        )
        evt = fprime_test_api.await_event(
            "ReferenceDeployment.modeManager.CurrentModeReading", timeout=2
        )
        if evt is not None and "SAFE_MODE" in str(evt.args[0].val).upper():
            fprime_test_api.send_command(
                "ReferenceDeployment.modeManager.EXIT_SAFE_MODE"
            )
    except Exception:
        pass


@pytest.fixture(scope="session", autouse=True)
def stop_radio(
    request: pytest.FixtureRequest, fprime_test_api_session: IntegrationTestAPI
):
    """Fixture to stop the radio at the end of the test session"""
    with_radio = request.config.getoption("--with-radio")

    yield

    if not with_radio:
        return

    fprime_test_api_session.send_command(
        command="ReferenceDeployment.lora.TRANSMIT", args=["DISABLED"]
    )

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
    fprime_test_api.send_and_assert_command(
        command="ReferenceDeployment.lora.TRANSMIT", args=["ENABLED"]
    )
    fprime_test_api.send_and_assert_command(
        command="ReferenceDeployment.downlinkDelay.DIVIDER_PRM_SET",
        args=[20],
    )


@pytest.fixture(autouse=True)
def start_radio(request: pytest.FixtureRequest, fprime_test_api: IntegrationTestAPI):
    """Fixture to start the radio before tests"""
    if not request.config.getoption("--with-radio"):
        return

    _enable_radio(fprime_test_api)


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

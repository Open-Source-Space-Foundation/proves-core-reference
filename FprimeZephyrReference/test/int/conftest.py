"""
conftest.py:

Pytest configuration for integration tests.
"""

import pytest
from fprime_gds.common.testing_fw.api import IntegrationTestAPI

TELEMETRY_DELAY = "ReferenceDeployment.telemetryDelay"


@pytest.fixture(scope="session")
def start_gds(fprime_test_api_session: IntegrationTestAPI):
    """Fixture to start GDS before tests and stop after tests

    GDS is used to send commands and receive telemetry/events.
    """
    # pro = subprocess.Popen(
    #     ["make", "gds-integration"],
    #     cwd=os.getcwd(),
    #     stdout=subprocess.PIPE,
    #     preexec_fn=os.setsid,
    # )
    #
    # gds_working = False
    # timeout_time = time.time() + 30
    # while time.time() < timeout_time:
    #     try:
    #         fprime_test_api_session.send_and_assert_command(
    #             command=f"{cmdDispatch}.CMD_NO_OP"
    #         )
    #         proves_send_and_assert_command(
    #             fprime_test_api_session, f"{TELEMETRY_DELAY}.DIVIDER_PRM_SET", [0]
    #         )
    #         gds_working = True
    #         break
    #     except Exception:
    #         time.sleep(1)
    # assert gds_working

    yield
    # os.killpg(os.getpgid(pro.pid), signal.SIGTERM)

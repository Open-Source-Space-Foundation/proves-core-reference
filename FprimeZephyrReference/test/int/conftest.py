"""
conftest.py:

Pytest configuration for integration tests.
"""

import threading
import time

import pytest
from common import cmdDispatch
from fprime_gds.common.testing_fw.api import IntegrationTestAPI
from fprime_gds.executables import run_deployment


def _run_gds():
    # Equivalent to: fprime-gds --gui=none
    args = run_deployment.parse_args(["--gui=none"])
    run_deployment.main(args)


@pytest.fixture(scope="session")
def start_gds(fprime_test_api_session: IntegrationTestAPI):
    """Fixture to start GDS before tests and stop after tests."""
    t = threading.Thread(target=_run_gds, daemon=True)
    t.start()

    gds_working = False
    timeout_time = time.time() + 30
    while time.time() < timeout_time:
        try:
            fprime_test_api_session.send_and_assert_command(
                command=f"{cmdDispatch}.CMD_NO_OP"
            )
            gds_working = True
            break
        except Exception:
            time.sleep(1)
    assert gds_working

    yield
    fprime_test_api_session.shutdown()

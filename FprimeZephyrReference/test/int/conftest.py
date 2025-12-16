"""
conftest.py:

Pytest configuration for integration tests.
"""

import os
import signal
import subprocess
import time

import pytest
from fprime_gds.common.testing_fw.api import IntegrationTestAPI

# Component mnemonics
CMD_DISPATCH = "ReferenceDeployment.cmdDisp"


def pytest_configure(config):
    """Register custom markers."""
    config.addinivalue_line("markers", "slow: marks tests as slow (5MB file transfers)")


@pytest.fixture(scope="session")
def start_gds(fprime_test_api_session: IntegrationTestAPI):
    """Fixture to start GDS before tests and stop after tests

    GDS is used to send commands and receive telemetry/events.
    """
    pro = subprocess.Popen(
        ["make", "gds-integration"],
        cwd=os.getcwd(),
        stdout=subprocess.PIPE,
        preexec_fn=os.setsid,
    )

    gds_working = False
    timeout_time = time.time() + 30
    while time.time() < timeout_time:
        try:
            fprime_test_api_session.send_and_assert_command(
                command=f"{CMD_DISPATCH}.CMD_NO_OP"
            )
            gds_working = True
            break
        except Exception:
            time.sleep(1)
    assert gds_working

    yield
    os.killpg(os.getpgid(pro.pid), signal.SIGTERM)

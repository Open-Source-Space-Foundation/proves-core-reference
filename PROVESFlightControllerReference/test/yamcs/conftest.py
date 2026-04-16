"""
conftest.py for YAMCS round-trip integration tests.

These tests assume the full YAMCS stack is already running:
  - YAMCS server reachable at YAMCS_URL (default localhost:8090)
  - Instance YAMCS_INSTANCE loaded (default fprime-project)
  - The PROVES adapter is bridging serial <-> UDP
  - The fprime-yamcs-events bridge is publishing FSW events into YAMCS

CI starts the stack via `make yamcs UART_DEVICE=/dev/ttyBOARD` before invoking
pytest; locally you can do the same and then run `make test-yamcs`.
"""

import os
import time

import pytest
from yamcs.client import YamcsClient

YAMCS_URL = os.environ.get("YAMCS_URL", "localhost:8090")
YAMCS_INSTANCE = os.environ.get("YAMCS_INSTANCE", "fprime-project")
YAMCS_PROCESSOR = os.environ.get("YAMCS_PROCESSOR", "realtime")
YAMCS_READY_TIMEOUT_S = float(os.environ.get("YAMCS_READY_TIMEOUT_S", "60"))


@pytest.fixture(scope="session")
def yamcs_client():
    """Session-scoped YAMCS client that blocks until the instance is RUNNING."""
    client = YamcsClient(YAMCS_URL)
    deadline = time.time() + YAMCS_READY_TIMEOUT_S
    last_error: Exception | None = None
    while time.time() < deadline:
        try:
            instance = client.get_instance(YAMCS_INSTANCE)
            if getattr(instance, "state", None) == "RUNNING":
                yield client
                return
        except Exception as exc:
            last_error = exc
        time.sleep(1.0)
    pytest.fail(
        f"YAMCS instance '{YAMCS_INSTANCE}' did not reach RUNNING state at "
        f"{YAMCS_URL} within {YAMCS_READY_TIMEOUT_S}s. Last error: {last_error!r}"
    )


@pytest.fixture(scope="session")
def yamcs_processor(yamcs_client):
    """Session-scoped realtime processor client for issuing TCs."""
    return yamcs_client.get_processor(
        instance=YAMCS_INSTANCE,
        processor=YAMCS_PROCESSOR,
    )

"""
sync_sequence_number.py:

This module syncs the sequence number used in authentication between
the F' component and the framing plugin by reading it from the component
and writing it to a file used by the plugin.
"""

import os
import subprocess
import time

import pytest
from fprime_gds.common.data_types.event_data import EventData
from fprime_gds.common.testing_fw.api import IntegrationTestAPI
from int.common import cmdDispatch, proves_send_and_assert_command


@pytest.fixture(scope="session", autouse=True)
def start_gds(fprime_test_api_session: IntegrationTestAPI):
    """Fixture to start GDS

    GDS is used to send commands and receive events
    """
    process = subprocess.Popen(["make", "gds-integration"], cwd=os.getcwd())

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
    process.kill()


def test_sync_sequence_number(fprime_test_api: IntegrationTestAPI):
    """Sync the authentication sequence number from the F' component to the framing plugin"""
    proves_send_and_assert_command(
        fprime_test_api, "ComCcsdsLora.authenticatelora.GET_SEQ_NUM"
    )
    evt: EventData = fprime_test_api.assert_event(
        "ComCcsdsLora.authenticatelora.EmitSequenceNumber", timeout=5
    )
    seq_num = evt.args[0].val

    with open("./Framing/src/sequence_number.bin", "w", encoding="utf-8") as f:
        f.write(str(seq_num))

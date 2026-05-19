"""
sync_sequence_number_test.py:

This module syncs the sequence number used in authentication between
the F' component and the framing plugin by reading it from the component
and writing it to a file used by the plugin.
"""

import pytest
from common import proves_send_and_assert_command
from fprime_gds.common.data_types.event_data import EventData
from fprime_gds.common.testing_fw.api import IntegrationTestAPI


@pytest.mark.sync_sequence_number
def test_sync_sequence_number(fprime_test_api: IntegrationTestAPI, start_gds):
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

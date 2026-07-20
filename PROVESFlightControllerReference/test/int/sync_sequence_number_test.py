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
def test_sync_sequence_number(
    fprime_test_api: IntegrationTestAPI, start_gds, request: pytest.FixtureRequest
):
    """Sync the authentication sequence number from the F' component to the framing plugin"""
    # Query the deframer on the link the upcoming traffic is validated by:
    # radio frames by the LoRa instance, UART frames by the UART instance.
    # Each instance keeps an independent runtime counter (they only share the
    # persistence file), so reading the wrong one regresses the GDS-side
    # counter and the next few commands are rejected as replays.
    link = request.config.getoption("--sync-deframer", default=None)
    if link is None:
        link = (
            "lora"
            if request.config.getoption("--with-radio", default=False)
            else "uart"
        )
    deframer = {
        "uart": "ComCcsdsUart.tcSecurityDeframer",
        "lora": "ComCcsdsLora.tcSecurityDeframer",
    }[link]
    proves_send_and_assert_command(fprime_test_api, f"{deframer}.GET_SEQ_NUM")
    evt: EventData = fprime_test_api.assert_event(
        f"{deframer}.SequenceNumberGet", timeout=5
    )
    seq_num = evt.args[0].val

    with open("./Framing/src/sequence_number.bin", "w", encoding="utf-8") as f:
        f.write(str(seq_num))

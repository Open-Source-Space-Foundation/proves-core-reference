"""
sync_sequence_number_test.py:

This module syncs the sequence number used in authentication between
the F' component and the framing plugin by reading it from the component
and writing it to a file used by the plugin.
"""

import pytest
from common import (
    latest_sequence_number,
    proves_send_and_assert_command,
    write_next_sequence_number,
)
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
    event = f"{deframer}.SequenceNumberGet"
    proves_send_and_assert_command(fprime_test_api, f"{deframer}.GET_SEQ_NUM")
    fprime_test_api.assert_event(event, timeout=5)

    # A radio retry of GET_SEQ_NUM can leave an earlier attempt's event in the
    # history, reporting a number lower than the board's last accepted one.
    # The latest reported number (wrap-aware) is the board's current one.
    is_sequence_number_get = fprime_test_api.get_event_pred(event)
    board_seq = latest_sequence_number(
        int(evt.args[0].val)
        for evt in fprime_test_api.get_event_test_history().retrieve()
        if is_sequence_number_get(evt)
    )
    write_next_sequence_number(board_seq)

"""
test_yamcs_noop.py:

End-to-end round-trip check through YAMCS:

  YAMCS issue_command  ->  proves_adapter (TC, auth)  ->  FSW
        FSW event  ->  fprime-yamcs-events bridge  ->  YAMCS event stream

If we can issue CMD_NO_OP via the YAMCS command processor and observe a
matching NoOpReceived event come back through the YAMCS event subscription,
the full TC + TM + events path is healthy.
"""

import queue
import time

NO_OP_COMMAND = "/ReferenceDeployment|ReferenceDeployment/CdhCore|cmdDisp|CMD_NO_OP"
NO_OP_EVENT_NEEDLE = "NoOpReceived"
EVENT_TIMEOUT_S = 30.0


def _event_matches(event) -> bool:
    """Return True if any textual field on a YAMCS Event contains the Noop needle."""
    for attr in ("event_type", "message", "source"):
        value = getattr(event, attr, None)
        if value and NO_OP_EVENT_NEEDLE in value:
            return True
    return False


def test_noop_round_trip(yamcs_client, yamcs_processor):
    """Issue CMD_NO_OP via YAMCS and assert a NoOpReceived event comes back."""
    events: "queue.Queue" = queue.Queue()

    subscription = yamcs_client.create_event_subscription(
        instance="fprime-project",
        on_data=events.put,
    )

    try:
        # Drain any events that arrived during subscription handshake so the
        # match below can only be triggered by the command we issue.
        time.sleep(0.5)
        while not events.empty():
            events.get_nowait()

        yamcs_processor.issue_command(NO_OP_COMMAND)

        deadline = time.time() + EVENT_TIMEOUT_S
        while time.time() < deadline:
            try:
                event = events.get(timeout=1.0)
            except queue.Empty:
                continue
            if _event_matches(event):
                return

        raise AssertionError(
            f"Did not observe a '{NO_OP_EVENT_NEEDLE}' event within "
            f"{EVENT_TIMEOUT_S}s of issuing {NO_OP_COMMAND}"
        )
    finally:
        subscription.cancel()

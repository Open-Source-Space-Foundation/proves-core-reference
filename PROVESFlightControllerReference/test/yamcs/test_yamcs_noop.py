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

NO_OP_COMMAND = "/CdhCore/cmdDisp/CMD_NO_OP"
NO_OP_EVENT_NEEDLE = "NoOpReceived"
TOTAL_TIMEOUT_S = 180.0
RETRY_INTERVAL_S = 10.0


def _event_matches(event) -> bool:
    """Return True if any textual field on a YAMCS Event contains the Noop needle."""
    for attr in ("event_type", "message", "source"):
        value = getattr(event, attr, None)
        if value and NO_OP_EVENT_NEEDLE in value:
            return True
    return False


def test_noop_round_trip(yamcs_client, yamcs_processor, yamcs_instance):
    """Issue CMD_NO_OP via YAMCS and assert a NoOpReceived event comes back.

    Retries the command every RETRY_INTERVAL_S until TOTAL_TIMEOUT_S elapses
    or the event is observed, mirroring GDS conftest's start_gds fixture.  A
    single TC can be dropped if FSW is mid-boot or in safe mode entry, so we
    keep nudging it until either the link comes up or we hit the hard ceiling.
    """
    events: queue.Queue = queue.Queue()

    subscription = yamcs_client.create_event_subscription(
        instance=yamcs_instance,
        on_data=events.put,
    )

    try:
        # Drain any events that arrived during subscription handshake so the
        # match below can only be triggered by the commands we issue.
        time.sleep(0.5)
        while not events.empty():
            events.get_nowait()

        deadline = time.time() + TOTAL_TIMEOUT_S
        attempts = 0
        next_issue = 0.0
        while time.time() < deadline:
            if time.time() >= next_issue:
                attempts += 1
                yamcs_processor.issue_command(NO_OP_COMMAND)
                next_issue = time.time() + RETRY_INTERVAL_S
            try:
                event = events.get(timeout=1.0)
            except queue.Empty:
                continue
            if _event_matches(event):
                return

        raise AssertionError(
            f"Did not observe a '{NO_OP_EVENT_NEEDLE}' event within "
            f"{TOTAL_TIMEOUT_S}s after {attempts} CMD_NO_OP attempts"
        )
    finally:
        subscription.cancel()

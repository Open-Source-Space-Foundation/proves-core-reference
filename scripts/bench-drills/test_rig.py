"""Offline test for the GDS log reader against real fprime-gds 4.3 lines."""

import tempfile
import unittest
from pathlib import Path

from rig import GdsLog

CHANNELS = (
    "TB_PROC_TIME: 212.453600s, context=0,(1(0)-212:453600),"
    "ReferenceDeployment.lora.BytesReceived,268500992,1234\n"
    "2026-10-18T14:30:00.000000+00:00,(2(0)-1792333800:0),"
    "ReferenceDeployment.startupManager.BootCount,268828672,57\n"
)
EVENTS = (
    "TB_PROC_TIME: 183.317400s, context=0,(1(0)-183:317400),"
    "CdhCore.cmdDisp.OpCodeDispatched,1,EventSeverity.COMMAND,"
    "Opcode 0x1000 dispatched to port 2, from a sequence\n"
)


class GdsLogTest(unittest.TestCase):
    def test_reads_names_when_time_has_a_comma(self):
        """Proc-time lines and wall-clock lines both key by channel and event name."""
        with tempfile.TemporaryDirectory() as d:
            log = GdsLog(d)
            (Path(d) / "channel.log").write_text(CHANNELS)
            (Path(d) / "event.log").write_text(EVENTS)
            log.poll()
            self.assertEqual(
                log.latest["ReferenceDeployment.lora.BytesReceived"], "1234"
            )
            self.assertEqual(
                log.latest["ReferenceDeployment.startupManager.BootCount"], "57"
            )
            self.assertEqual(
                log.events,
                [
                    (
                        "CdhCore.cmdDisp.OpCodeDispatched",
                        "Opcode 0x1000 dispatched to port 2, from a sequence",
                    )
                ],
            )


if __name__ == "__main__":
    unittest.main()

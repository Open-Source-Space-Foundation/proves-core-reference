"""Offline tests for the rig: GDS log reading and the universal-gate snapshot."""

import subprocess
import tempfile
import unittest
from pathlib import Path

from gate import universal_gate, verdict
from rig import Bench, Dictionary, Drill, DryRig, GdsEvent, GdsLog, run_drill

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
CONFIG_FAILED = "ReferenceDeployment.uspRadio.ConfigurationFailed"


class GdsLogTest(unittest.TestCase):
    def test_reads_names_when_time_has_a_comma(self):
        """Proc-time lines and wall-clock lines both key by name, with board time."""
        with tempfile.TemporaryDirectory() as d:
            log = GdsLog(d)
            (Path(d) / "channel.log").write_text(CHANNELS)
            (Path(d) / "event.log").write_text(EVENTS)
            log.poll()
            name = "ReferenceDeployment.lora.BytesReceived"
            self.assertEqual(log.latest[name], "1234")
            self.assertEqual(log.stamp[name], 212.4536)
            self.assertEqual(
                log.latest["ReferenceDeployment.startupManager.BootCount"], "57"
            )
            self.assertEqual(
                log.events,
                [
                    GdsEvent(
                        "CdhCore.cmdDisp.OpCodeDispatched",
                        "EventSeverity.COMMAND",
                        "Opcode 0x1000 dispatched to port 2, from a sequence",
                    )
                ],
            )


def dry_rig(run_dir: str, probe: str = "none") -> DryRig:
    bench = Bench({"_PATH": "test.env", "FLIGHT_PROBE_SERIAL": probe})
    return DryRig(bench, ("flight",), Path(run_dir))


class SnapshotTest(unittest.TestCase):
    def test_throttled_event_is_unreadable(self):
        """ConfigurationFailed at its throttle (2) can no longer move: FAIL, with the reason."""
        with tempfile.TemporaryDirectory() as d:
            rig = dry_rig(d)
            events = rig.logs["flight"].events
            events.append(GdsEvent(CONFIG_FAILED, "WARNING_HI", "x"))
            before = rig.snapshot(rig.update_marks())
            events.append(GdsEvent(CONFIG_FAILED, "WARNING_HI", "x"))
            after = rig.snapshot(rig.update_marks())
            self.assertEqual(before.ends["flight"].config_failed, 1)
            self.assertIsNone(after.ends["flight"].config_failed)
            checks = universal_gate(before, after, rig.tally)
            row = next(c for c in checks if "ConfigurationFailed" in c.name)
            self.assertFalse(row.passed)
            self.assertIn("hit its throttle (2)", row.detail)

    def test_silent_gds_is_unreadable_not_delta_zero(self):
        """A GDS that logs nothing during the settle fails the gate instead of reading flat."""
        with tempfile.TemporaryDirectory() as d:
            rig = dry_rig(d)
            before = rig.snapshot(rig.update_marks())
            rig.logs["flight"].live = False
            after = rig.snapshot(rig.update_marks())
            self.assertEqual(before.ends["flight"].boot_count, 57)
            self.assertIsNone(after.ends["flight"].boot_count)
            self.assertFalse(verdict(universal_gate(before, after, rig.tally)))

    def test_dead_openocd_is_unreadable(self):
        """A probe whose openocd does not answer gives halted=None, not a log count."""
        with tempfile.TemporaryDirectory() as d:
            rig = dry_rig(d, probe="E6647C74034E602F")
            snap = rig.snapshot(rig.update_marks())
            self.assertIsNone(snap.ends["flight"].halted)
            self.assertIn("targets", snap.ends["flight"].why["halted"])


class RadioInstanceTest(unittest.TestCase):
    def test_legacy_build_resolves_to_lora(self):
        d = Dictionary({"commands": [{"name": "R.lora.TRANSMIT", "opcode": 1}]})
        self.assertEqual(d.radio_instance(), "lora")


class CrashPathTest(unittest.TestCase):
    def test_crash_in_drill_is_a_fail_with_evidence(self):
        """A refused socket mid-drill records FAIL and still writes evidence.md."""

        def boom(rig):
            raise ConnectionRefusedError("openocd telnet")

        def hang(rig):
            raise subprocess.TimeoutExpired("fprime-cli", 60)

        bench = Bench({"_PATH": "test.env", "FLIGHT_PROBE_SERIAL": "none"})
        for run, error in ((boom, "ConnectionRefusedError"), (hang, "TimeoutExpired")):
            with tempfile.TemporaryDirectory() as d:
                drill = Drill("DX", "crash", ("flight",), run, (), build="none")
                self.assertFalse(run_drill(drill, bench, d, {}, dry=True))
                (evidence,) = Path(d).glob("*/*-DX-dry/evidence.md")
                self.assertIn(
                    f"| drill ran to the end | FAIL | {error}", evidence.read_text()
                )


if __name__ == "__main__":
    unittest.main()

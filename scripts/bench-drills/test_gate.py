"""Offline tests for the universal gate with canned counter values.

Run: python3 -m unittest discover -s scripts/bench-drills -p 'test_*.py'
"""

import unittest

from gate import (
    CommandTally,
    Criterion,
    EndCounters,
    Snapshot,
    evaluate,
    render_evidence,
    universal_gate,
    verdict,
)

QUIET_FLIGHT = EndCounters(
    config_failed=0, rx_overrun=0, rx_dropped=0, halted=0, boot_count=57
)
QUIET_GRC = EndCounters(config_failed=0, rx_overrun=0, rx_dropped=0, halted=0)


def tally(confirmed=3):
    t = CommandTally()
    for i in range(3):
        t.record("flight", f"cmd{i}", i < confirmed)
    return t


class UniversalGateTest(unittest.TestCase):
    def test_quiet_run_passes(self):
        before = Snapshot({"flight": QUIET_FLIGHT, "grc": QUIET_GRC})
        after = Snapshot({"flight": QUIET_FLIGHT, "grc": QUIET_GRC})
        checks = universal_gate(before, after, tally())
        self.assertTrue(verdict(checks), [c for c in checks if not c.passed])

    def test_flight_reboot_fails(self):
        rebooted = EndCounters(
            config_failed=0, rx_overrun=0, rx_dropped=0, halted=0, boot_count=58
        )
        before = Snapshot({"flight": QUIET_FLIGHT, "grc": QUIET_GRC})
        after = Snapshot({"flight": rebooted, "grc": QUIET_GRC})
        checks = universal_gate(before, after, tally())
        failed = [c.name for c in checks if not c.passed]
        self.assertEqual(failed, ["flight BootCount delta = 0"])
        self.assertFalse(verdict(checks))

    def test_unreadable_counter_fails(self):
        blind = EndCounters(config_failed=0, rx_overrun=0, rx_dropped=None, halted=0)
        before = Snapshot({"grc": blind})
        checks = universal_gate(before, before, tally())
        self.assertEqual(
            [c.name for c in checks if not c.passed], ["grc RxDropped delta = 0"]
        )

    def test_probeless_end_reports_halted_as_na(self):
        """No probe: the halted row is N/A, visible in the table, and not a PASS."""
        probeless = EndCounters(0, 0, 0, None, has_probe=False)
        before = Snapshot({"flight": QUIET_FLIGHT, "grc": probeless})
        checks = universal_gate(before, before, tally())
        row = next(c for c in checks if c.name == "grc openocd halted delta = 0")
        self.assertTrue(row.na)
        self.assertFalse(row.passed)
        self.assertTrue(verdict(checks))
        text = render_evidence("D1", "t", [], before, before, checks, [], "/tmp/x")
        self.assertIn("| grc openocd halted delta = 0 | N/A |", text)

    def test_probeless_board_byte_counter_drop_fails(self):
        """A GRC whose BytesSent goes down rebooted, even with no probe or BootCount."""
        b = EndCounters(0, 0, 0, None, has_probe=False, traffic=(900, 400))
        a = EndCounters(0, 0, 0, None, has_probe=False, traffic=(120, 410))
        checks = universal_gate(Snapshot({"grc": b}), Snapshot({"grc": a}), tally())
        row = next(c for c in checks if "reboot detector" in c.name)
        self.assertFalse(row.passed)
        self.assertFalse(verdict(checks))

    def test_unconfirmed_or_missing_command_fails(self):
        before = Snapshot({"grc": QUIET_GRC})
        t = tally(confirmed=3)
        t.record_missing("flight", "SET_RX_PROFILE_UNGUARDED")
        checks = universal_gate(before, before, t)
        self.assertEqual(
            [c.detail for c in checks if not c.passed],
            ["3/4 confirmed; not in dictionary flight:SET_RX_PROFILE_UNGUARDED"],
        )
        checks = universal_gate(before, before, tally(confirmed=2))
        self.assertFalse(verdict(checks))


class CriteriaTest(unittest.TestCase):
    def test_threshold_and_missing_measurement(self):
        criteria = (
            Criterion("goodput", "kbps", ">= 19.5 kbps", lambda v: v >= 19.5),
            Criterion("parity", "parity", "equal", lambda v: v),
            Criterion("sweep", "sweep", "recorded", gated=False),
        )
        checks = evaluate(criteria, {"kbps": 19.4})
        self.assertEqual([c.passed for c in checks], [False, False, True])
        checks = evaluate(criteria, {"kbps": 21.6, "parity": True})
        self.assertTrue(verdict(checks))

    def test_evidence_carries_bootcount_and_verdict(self):
        before = Snapshot({"flight": QUIET_FLIGHT})
        checks = universal_gate(before, before, tally())
        text = render_evidence(
            "D2",
            "128 s regression",
            [("PCR sha", "abc")],
            before,
            before,
            checks,
            [],
            "/tmp/x",
        )
        self.assertIn("### D2: 128 s regression (PASS)", text)
        self.assertIn("| flight BootCount before / after | 57 / 57 |", text)


if __name__ == "__main__":
    unittest.main()

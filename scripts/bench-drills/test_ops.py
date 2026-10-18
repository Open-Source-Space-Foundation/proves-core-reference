"""Offline test for the D4 parser against Zephyr's `kernel thread list` format."""

import re
import unittest

from ops import cpu_share

LISTING = (
    "\x1b[1;32muart:~$ \x1b[mkernel thread list\r\n"
    "Scheduler: 12 since last call\r\n"
    "Threads:\r\n"
    "*0x20001000 uspRadio  \r\n"
    "\toptions: 0x0, priority: 5 timeout: 0\r\n"
    "\tTotal execution cycles: {radio} (1 %)\r\n"
    " 0x20005000 lbm_main_thread_id\r\n"
    "\tTotal execution cycles: {lbm} (0 %)\r\n"
    " 0x20002000 NA        \r\n"
    "\tTotal execution cycles: {na1} (0 %)\r\n"
    " 0x20003000 NA        \r\n"
    "\tTotal execution cycles: {na2} (0 %)\r\n"
    " 0x20004000 idle      \r\n"
    "\tTotal execution cycles: {idle} (98 %)\r\n"
)
WRAP = 2**32


class FakeShellRig:
    """Serves one canned listing per shell call and keeps what the drill saves."""

    def __init__(self, samples):
        self.samples = list(samples)
        self.saved = {}

    def shell(self, end, line, wait=3.0):
        """Next canned listing."""
        return LISTING.format(**self.samples.pop(0))

    def save(self, name, text):
        """Keep the artifact."""
        self.saved[name] = text

    def now(self):
        """No clock."""
        return 0.0

    def sleep(self, seconds):
        """No clock."""

    def log(self, message):
        """Discard."""


def samples(per_interval, start, n=4):
    """n uint32 readings advancing by `per_interval` each step."""
    return [
        {k: (start[k] + i * per_interval[k]) % WRAP for k in start} for i in range(n)
    ]


class CpuShareTest(unittest.TestCase):
    def test_idle_wraps_more_than_once_across_the_minute(self):
        """Idle wraps twice in 60 s; per-20 s deltas still give the true share."""
        step = {
            "radio": 3 * 10**8,
            "lbm": 10**8,
            "na1": 0,
            "na2": 0,
            "idle": 32 * 10**8,
        }
        start = {"radio": 5, "lbm": 0, "na1": 0, "na2": 0, "idle": WRAP - 10}
        rig = FakeShellRig(samples(step, start))
        # (3e8 + 1e8) * 3 / ((3e8 + 1e8 + 3.2e9) * 3) = 11.11 %
        self.assertEqual(cpu_share(rig, "P0"), 11.11)
        self.assertIn("threads-P0-0.txt", rig.saved)
        self.assertIn("lbm_main_thread_id", rig.saved["cpu-P0.txt"])

    def test_missing_thread_is_unmeasured(self):
        """No radio-like thread in the listing gives None, not 0 %."""
        listing = {"radio": 0, "lbm": 0, "na1": 0, "na2": 0, "idle": 10}
        rig = FakeShellRig([listing] * 4)
        self.assertIsNone(cpu_share(rig, "P0", threads=re.compile("comQueue")))


if __name__ == "__main__":
    unittest.main()

"""Offline test for the D4 parser against Zephyr's `kernel thread list` format."""

import unittest

from ops import cpu_share

LISTING = (
    "\x1b[1;32muart:~$ \x1b[mkernel thread list\r\n"
    "Scheduler: 12 since last call\r\n"
    "Threads:\r\n"
    "*0x20001000 uspRadio  \r\n"
    "\toptions: 0x0, priority: 5 timeout: 0\r\n"
    "\tTotal execution cycles: {radio} (1 %)\r\n"
    " 0x20002000 NA        \r\n"
    "\tTotal execution cycles: {na1} (0 %)\r\n"
    " 0x20003000 NA        \r\n"
    "\tTotal execution cycles: {na2} (0 %)\r\n"
    " 0x20004000 idle      \r\n"
    "\tTotal execution cycles: {idle} (98 %)\r\n"
)


class FakeShellRig:
    """Serves two canned listings, one per cpu_share sample."""

    def __init__(self, samples):
        self.samples = list(samples)

    def shell(self, end, line, wait=3.0):
        """Next canned listing."""
        return LISTING.format(**self.samples.pop(0))

    def sleep(self, seconds):
        """No clock."""

    def log(self, message):
        """Discard."""


class CpuShareTest(unittest.TestCase):
    def test_share_with_wrap_and_unnamed_threads(self):
        """uint32 wrap on idle and two same-named threads still give 4 %."""
        before = {"radio": 1000, "na1": 0, "na2": 0, "idle": 2**32 - 500}
        after = {"radio": 5000, "na1": 1000, "na2": 1000, "idle": 93500}
        self.assertEqual(cpu_share(FakeShellRig([before, after])), 4.0)

    def test_missing_thread_is_unmeasured(self):
        """No uspRadio thread in the listing gives None, not 0 %."""
        sample = {"radio": 0, "na1": 0, "na2": 0, "idle": 10}
        rig = FakeShellRig([sample, {**sample, "idle": 20}])
        self.assertIsNone(cpu_share(rig, thread="comQueue"))


if __name__ == "__main__":
    unittest.main()

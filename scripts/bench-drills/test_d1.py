"""Offline test for the D1d carrier guard."""

import unittest

from d1 import run_d1d
from rig import Dictionary


def cw_dictionary(name):
    return Dictionary(
        {
            "commands": [
                {
                    "name": "ReferenceDeployment.lora.CONTINUOUS_WAVE",
                    "opcode": 1,
                    "formalParams": [{"name": name, "type": {"name": "U16"}}],
                }
            ]
        }
    )


class GuardRig:
    """Records every command; dispatch and outcomes are canned."""

    ends = ("flight",)

    def __init__(self, dictionary):
        self.dicts = {"flight": dictionary}
        self.sent = []

    def command(self, end, short, *args, **kw):
        self.sent.append((short, args))

        class Sent:
            confirmed = True

        return Sent()

    def outcome(self, sent, wait=15):
        return "completed"

    def log(self, message):
        """Discard."""


class CwGuardTest(unittest.TestCase):
    def test_seconds_argument_keys_no_carrier(self):
        """`seconds: U16` (lora, UspRadio today): no CONTINUOUS_WAVE is sent at all."""
        rig = GuardRig(cw_dictionary("seconds"))
        out = run_d1d(rig)
        self.assertEqual([s for s, _ in rig.sent if s == "CONTINUOUS_WAVE"], [])
        self.assertEqual(out, {"cw argument": "seconds: U16"})

    def test_milliseconds_argument_sends_out_of_range_first(self):
        rig = GuardRig(cw_dictionary("duration_ms"))
        run_d1d(rig)
        cw = [args[0] for s, args in rig.sent if s == "CONTINUOUS_WAVE"]
        self.assertEqual(cw, [0, 10001, 1, 10000])


if __name__ == "__main__":
    unittest.main()

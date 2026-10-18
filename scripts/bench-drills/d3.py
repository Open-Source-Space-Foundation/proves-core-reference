#!/usr/bin/env python3
"""D3 P3 saturation and D3b goodput sweep, July's r5 method (#593)."""

from __future__ import annotations

import sys

from gate import Criterion
from ops import saturation
from rig import Drill, main

BOTH = ("flight", "grc")
D3_FLOOR_KBPS = 19.5  # 90% of July's 21.63 kbps
SWEEP = ("P0_LORA_SF8", "P1_LORA_SF10", "P2_LORA_SF7")

DRILLS = [
    Drill(
        "D3",
        "P3 saturation",
        BOTH,
        lambda rig: saturation(rig, "P3_GFSK_38K"),
        (
            Criterion(
                "P3 goodput",
                "goodput_kbps",
                f">= {D3_FLOOR_KBPS} kbps",
                lambda v: v >= D3_FLOOR_KBPS,
            ),
            Criterion("measurement window", "window_s", ">= 120 s", lambda v: v >= 120),
        ),
    ),
    Drill(
        "D3b",
        "Goodput sweep",
        BOTH,
        lambda rig: {p: saturation(rig, p)["goodput_kbps"] for p in SWEEP},
        tuple(
            Criterion(f"{p} goodput (kbps)", p, "recorded", gated=False) for p in SWEEP
        ),
    ),
]

if __name__ == "__main__":
    sys.exit(main(DRILLS))

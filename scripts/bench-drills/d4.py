#!/usr/bin/env python3
"""D4: uspRadio idle CPU share over the RTT shell (#593)."""

from __future__ import annotations

import sys

from gate import Criterion
from ops import cpu_share, quiesce, restore_boot_profiles, set_profile
from rig import Drill, Rig, main

CPU_CEILING_PCT = 5.0
IDLE_PROFILES = ("P0_LORA_SF8", "P3_GFSK_38K")


def run(rig: Rig) -> dict:
    """No traffic, RX on P0 then P3: delta uspRadio cycles / delta total cycles over 60 s."""
    rig.start_rtt_shell()
    quiesce(rig)
    out = {}
    for profile in IDLE_PROFILES:
        set_profile(rig, "flight", "RX", profile, revert_s=None)
        out[profile] = cpu_share(rig, profile, gap=rig.param("gap_s", 60))
        rig.log(f"uspRadio share on {profile}: {out[profile]} %")
    restore_boot_profiles(rig)
    return out


DRILLS = [
    Drill(
        "D4",
        "uspRadio idle CPU",
        ("flight",),
        run,
        tuple(
            Criterion(
                f"uspRadio share, RX {p}",
                p,
                f"< {CPU_CEILING_PCT} %",
                lambda v: v < CPU_CEILING_PCT,
            )
            for p in IDLE_PROFILES
        ),
        keys=("FLIGHT_RTT_SHELL_PORT",),
    )
]

if __name__ == "__main__":
    sys.exit(main(DRILLS))

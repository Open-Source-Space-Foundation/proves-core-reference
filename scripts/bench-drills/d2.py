#!/usr/bin/env python3
"""D2: the 128 s regression check for rp_disable_failsafe() (#593)."""

from __future__ import annotations

import sys

from gate import Criterion
from ops import quiesce, restore_boot_profiles, set_profile, transmit
from rig import Drill, Rig, main

SWITCH_PROFILES = (
    "P1_LORA_SF10",
    "P2_LORA_SF7",
    "P3_GFSK_38K",
    "P1_LORA_SF10",
    "P0_LORA_SF8",
)


def run(rig: Rig) -> dict:
    """Uptime past 150 s, 5 TRANSMIT cycles, 5 profile switches, then idle RX.

    There is no uptime channel, so uptime > 150 s is proven by watching
    BootCount stay put for 150 s plus the Beacon settle."""
    boot0 = rig.channel_int("flight", "startupManager.BootCount")
    rig.sleep(150)
    rig.settle(("flight",))
    boot1 = rig.channel_int("flight", "startupManager.BootCount")
    up = boot0 is not None and boot0 == boot1
    for _ in range(5):
        transmit(rig, "flight", True)
        transmit(rig, "flight", False)
    for profile in SWITCH_PROFILES:
        set_profile(rig, "flight", "TX", profile)
        set_profile(rig, "flight", "RX", profile)
    restore_boot_profiles(rig)
    quiesce(rig)
    idle_min = rig.param("idle_min", 10)
    rig.log(f"idle RX for {idle_min} min")
    rig.sleep(idle_min * 60)
    return {"uptime_over_150s": up, "idle_min": idle_min}


DRILLS = [
    Drill(
        "D2",
        "128 s regression",
        ("flight",),
        run,
        (
            Criterion(
                "uptime > 150 s before commanding",
                "uptime_over_150s",
                "True",
                lambda v: v is True,
            ),
            Criterion(
                "idle RX long enough", "idle_min", ">= 10 min", lambda v: v >= 10
            ),
        ),
    )
]

if __name__ == "__main__":
    sys.exit(main(DRILLS))

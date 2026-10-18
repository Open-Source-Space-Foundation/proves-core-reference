#!/usr/bin/env python3
"""D5: multi-hour soak, July's soak5-duration-driver.sh cycle (#593)."""

from __future__ import annotations

import sys

from gate import Criterion
from ops import (
    cpu_share,
    profile_events,
    quiesce,
    restore_boot_profiles,
    saturation,
    set_profile,
)
from rig import BOOT_PROFILE, Drill, Rig, main

SOAK_HOURS = 6
CPU_CEILING_PCT = 5.0
SWITCHES = 10


def revert_drill(rig: Rig) -> bool:
    """Flight RX to P3 with revert_s=30 and no traffic: back on P0 within 45 s."""
    quiesce(rig)
    if not set_profile(rig, "flight", "RX", "P3_GFSK_38K", revert_s=30):
        return False
    text = f"RX profile changed to {BOOT_PROFILE}"
    base = profile_events(rig, "flight", "ProfileChanged", text)
    return rig.wait_for(
        lambda: profile_events(rig, "flight", "ProfileChanged", text) > base,
        30 + 45,
        every=5,
    )


def cycle(rig: Rig, idle_s: float) -> dict:
    """Idle RX (D4 sampled), 10 switches per end, r5 flood, revert drill, quiesce."""
    quiesce(rig)
    share = cpu_share(rig, gap=60)
    rig.sleep(max(0, idle_s - 60))
    for i in range(SWITCHES):
        profile = ("P3_GFSK_38K", "P1_LORA_SF10")[i % 2]
        set_profile(rig, "flight", "TX", profile)
        set_profile(rig, "grc", "RX", profile)
    restore_boot_profiles(rig)
    goodput = saturation(rig, "P3_GFSK_38K")["goodput_kbps"]
    reverted = revert_drill(rig)
    restore_boot_profiles(rig)
    quiesce(rig)
    return {"cpu_share": share, "goodput_kbps": goodput, "reverted": reverted}


def run(rig: Rig) -> dict:
    """Repeat the cycle until the soak duration has passed."""
    hours = rig.param("hours", SOAK_HOURS)
    idle_s = rig.param("idle_min", 30) * 60
    rig.start_rtt_shell()
    start, cycles = rig.now(), []
    while rig.now() - start < hours * 3600:
        cycles.append(cycle(rig, idle_s))
        rig.log(f"cycle {len(cycles)}: {cycles[-1]}")
    shares = [c["cpu_share"] for c in cycles]
    return {
        "hours": round((rig.now() - start) / 3600, 2),
        "cycles": len(cycles),
        "max_cpu_share": None if None in shares or not shares else max(shares),
        "reverts_ok": f"{sum(c['reverted'] for c in cycles)}/{len(cycles)}",
        "goodput_kbps": [c["goodput_kbps"] for c in cycles],
    }


def all_ok(v: str) -> bool:
    """'n/n' with n > 0."""
    ok, total = v.split("/")
    return ok == total != "0"


DRILLS = [
    Drill(
        "D5",
        "Soak",
        ("flight", "grc"),
        run,
        (
            Criterion(
                "soak duration",
                "hours",
                f">= {SOAK_HOURS} h",
                lambda v: v >= SOAK_HOURS,
            ),
            Criterion("revert drill every cycle", "reverts_ok", "n/n", all_ok),
            Criterion(
                "D4 in every idle window",
                "max_cpu_share",
                f"< {CPU_CEILING_PCT} %",
                lambda v: v < CPU_CEILING_PCT,
            ),
            Criterion("r5 goodput per cycle", "goodput_kbps", "recorded", gated=False),
        ),
        keys=("FLIGHT_RTT_SHELL_PORT",),
    )
]

if __name__ == "__main__":
    sys.exit(main(DRILLS))

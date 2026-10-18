#!/usr/bin/env python3
"""D6: legacy-GRC P0 interop (#593).

Ends: `flight` (its UART GDS, the truth for dispatch), `legacy` (the legacy
GRC from GRC main, SX1276), and `rf` (a GDS on the flight dictionary talking
through the legacy GRC's data CDC, i.e. over the air).
"""

from __future__ import annotations

import sys

from gate import Criterion
from ops import transmit
from rig import Drill, Rig, main

COMMANDS = 10
BEACON_WINDOW_S = 120
BEACON = "startupManager.BootCount"


def run(rig: Rig) -> dict:
    """(a) legacy GRC -> flight NO_OPs dispatched; (b) flight Beacons decoded via legacy GRC.

    (b) counts Beacons (BootCount samples) on the rf GDS against the flight
    UART GDS over the same TX-enabled window. The UART sees every Beacon, so
    it is the denominator."""
    flag = next(
        (
            v
            for k, v in rig.dicts["flight"].constants.items()
            if k.endswith("USP_RADIO_RADIOHEAD_HEADER")
        ),
        None,
    )
    noop = rig.dicts["flight"].find(rig.dicts["flight"].commands, "CMD_NO_OP")
    dispatched = 0
    if noop:
        opcode = rig.dicts["flight"].commands[noop]
        for _ in range(COMMANDS):
            base = rig.opcode_events("flight", "OpCodeDispatched", opcode)
            sent = rig.command(
                "rf",
                "CMD_NO_OP",
                effect=lambda b=base: (
                    rig.opcode_events("flight", "OpCodeDispatched", opcode) > b
                ),
                timeout=60,
            )
            dispatched += sent.confirmed
    transmit(rig, "flight", True)
    uart0, rf0 = (
        rig.channel_updates("flight", BEACON),
        rig.channel_updates("rf", BEACON),
    )
    rig.sleep(BEACON_WINDOW_S)
    uart = rig.channel_updates("flight", BEACON) - uart0
    transmit(rig, "flight", False)
    rig.sleep(10)
    rf = rig.channel_updates("rf", BEACON) - rf0
    rig.log(f"Beacons: rf {rf}, flight UART {uart} in {BEACON_WINDOW_S} s")
    return {
        "radiohead_header": flag,
        "noop_dispatched": f"{dispatched}/{COMMANDS}",
        "beacons_rf_vs_uart": (rf, uart),
    }


DRILLS = [
    Drill(
        "D6",
        "Legacy-GRC P0 interop",
        ("flight", "legacy", "rf"),
        run,
        (
            Criterion(
                "flight built with RadioHead header",
                "radiohead_header",
                "true",
                lambda v: v in (True, "true"),
            ),
            Criterion(
                "(a) legacy GRC -> flight dispatched",
                "noop_dispatched",
                f"{COMMANDS}/{COMMANDS}",
                lambda v: v == f"{COMMANDS}/{COMMANDS}",
            ),
            # One Beacon can straddle the window edge on either GDS.
            Criterion(
                "(b) legacy GDS decodes the flight Beacons",
                "beacons_rf_vs_uart",
                "(rf, uart): uart >= 2 and rf >= uart - 1",
                lambda v: v[1] >= 2 and v[0] >= v[1] - 1,
            ),
        ),
        same_pins=False,
    )
]

if __name__ == "__main__":
    sys.exit(main(DRILLS))

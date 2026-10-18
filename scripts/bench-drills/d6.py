#!/usr/bin/env python3
"""D6: legacy-GRC P0 interop (#593).

Ends: `flight` (its UART GDS, the truth for dispatch), `legacy` (the legacy
GRC from GRC main, SX1276), and `rf` (a GDS on the flight dictionary talking
through the legacy GRC's data CDC, i.e. over the air).
"""

from __future__ import annotations

import re
import sys
from pathlib import Path

from gate import Criterion
from ops import transmit
from rig import Drill, Rig, main

COMMANDS = 10
BEACON_WINDOW_S = 120
FRAMING_ERROR = re.compile(
    r"(?i)(defram|framing|checksum|crc).*(error|fail|invalid)|(error|fail|invalid).*(defram|framing)"
)
GDS_DATA_LOGS = {"channel.log", "event.log", "command.log"}


def framing_errors(log_dir: str) -> int:
    """Framing complaints in the rf GDS's own logs (not the decoded data logs)."""
    hits = 0
    for path in Path(log_dir).glob("*.log"):
        if path.name not in GDS_DATA_LOGS:
            hits += sum(
                1
                for line in path.read_text(errors="replace").splitlines()
                if FRAMING_ERROR.search(line)
            )
    return hits


def run(rig: Rig) -> dict:
    """(a) legacy GRC -> flight NO_OPs dispatched; (b) flight Beacons decoded via legacy GRC."""
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
    framing0 = framing_errors(rig.bench.get("rf", "GDS_LOGS"))
    beacons0 = rig.channel_updates("rf", "startupManager.BootCount")
    transmit(rig, "flight", True)
    rig.sleep(BEACON_WINDOW_S)
    transmit(rig, "flight", False)
    rig.sleep(10)
    return {
        "radiohead_header": flag,
        "noop_dispatched": f"{dispatched}/{COMMANDS}",
        "beacons_decoded": rig.channel_updates("rf", "startupManager.BootCount")
        - beacons0,
        "framing_errors": framing_errors(rig.bench.get("rf", "GDS_LOGS")) - framing0,
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
            Criterion(
                "(b) Beacons decoded by legacy GDS",
                "beacons_decoded",
                ">= 1",
                lambda v: v >= 1,
            ),
            Criterion("(b) no framing errors", "framing_errors", "0", lambda v: v == 0),
        ),
        same_pins=False,
    )
]

if __name__ == "__main__":
    sys.exit(main(DRILLS))

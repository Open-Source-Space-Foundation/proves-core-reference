#!/usr/bin/env python3
"""Run the drill suite. With no drill ids, runs every drill that shares the
standard bench and the overlay build; D1s (plain build) and D6 (legacy GRC)
need a reflash or a different GRC, so they run only when named."""

from __future__ import annotations

import sys

import d1
import d2
import d3
import d4
import d5
import d6
from rig import main

ALL = d1.DRILLS + d2.DRILLS + d3.DRILLS + d4.DRILLS + d6.DRILLS + d5.DRILLS
SUITE = [d.id for d in ALL if d.build == "overlay" and d.same_pins]

if __name__ == "__main__":
    argv = sys.argv[1:]
    named = [a for a in argv if not a.startswith("-") and a in {d.id for d in ALL}]
    if not named and not any(a in ("-h", "--help") for a in argv):
        argv = SUITE + argv
    sys.exit(main(ALL, argv))

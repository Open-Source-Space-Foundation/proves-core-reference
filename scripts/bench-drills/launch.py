#!/usr/bin/env python3
"""Print the openocd and GDS commands a bench file implies, one end at a time.

The drills read the logs these commands write. Nothing here touches a device.
"""

from __future__ import annotations

import argparse
import shlex
import sys
from pathlib import Path

from rig import DEFAULT_BENCH, ENDS, Bench, BenchError, load_bench


def main() -> int:
    """Print launch commands, or the bench keys that block them."""
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument(
        "ends",
        nargs="*",
        default=["flight", "grc"],
        help=f"ends to print ({', '.join(ENDS)})",
    )
    p.add_argument(
        "--bench",
        type=Path,
        default=DEFAULT_BENCH,
        help="bench env file (default: %(default)s)",
    )
    args = p.parse_args()
    bench, status = Bench(load_bench(args.bench)), 0
    for end in args.ends:
        print(f"# {end}")
        problems = bench.problems((end,))
        for problem in problems:
            print(f"#   BLOCKED: {problem}")
        if problems:
            status = 2
            continue
        if bench.has_probe(end):
            print(shlex.join(bench.openocd_command(end)))
        print(f"mkdir -p {shlex.quote(bench.get(end, 'GDS_LOGS'))}")
        print(
            f"cd {shlex.quote(bench.get(end, 'REPO'))} && {shlex.join(bench.gds_command(end))}"
        )
    return status


if __name__ == "__main__":
    try:
        sys.exit(main())
    except BenchError as e:
        print(f"BENCH ERROR: {e}", file=sys.stderr)
        sys.exit(2)

#!/usr/bin/env python3
"""Turn console-*.log CSV rows into one Markdown table. Usage: table.py results/console-*.log"""

import sys

rows = []
for path in sys.argv[1:]:
    for line in open(path):
        if line.startswith("CSV,"):
            rows.append(line.strip().split(",")[1:])
hdr = [
    "mode",
    "pass",
    "wr_hz",
    "rd_hz",
    "actual_hz",
    "n",
    "rd_min_us",
    "rd_med_us",
    "rd_p99_us",
    "rd_max_us",
    "cpu_med_us",
    "cpu_p99_us",
    "busy_med_us",
    "busy_max_us",
    "bad_xfers",
    "bad_bytes",
    "spi_errs",
    "busy_timeouts",
]
rows.sort(key=lambda r: (r[1], int(r[3]), r[0]))
print("| " + " | ".join(hdr) + " |")
print("|" + "---|" * len(hdr))
for r in rows:
    print("| " + " | ".join(r) + " |")

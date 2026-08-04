#!/usr/bin/env python3
"""Decode binary MOSAIC sample files downlinked from the spacecraft."""

import argparse
import csv
import struct
import sys
from datetime import UTC, datetime
from pathlib import Path

# MosaicManager writes native RP2350 values in this order. The RP2350 target is
# little-endian, so each record is 8 bytes: U32 seconds, U16 ADC, U16 millivolts.
RECORD = struct.Struct("<IHH")


def decode_file(path: Path) -> list[tuple[int, int, int]]:
    """Return all complete MOSAIC records in path."""
    data = path.read_bytes()
    if len(data) % RECORD.size != 0:
        raise ValueError(
            f"{path}: {len(data)} bytes is not a whole number of "
            f"{RECORD.size}-byte records; the file may be truncated"
        )
    return list(RECORD.iter_unpack(data))


def utc_timestamp(seconds: int) -> str:
    """Format Unix seconds as an ISO 8601 UTC timestamp."""
    return datetime.fromtimestamp(seconds, UTC).isoformat().replace("+00:00", "Z")


def display_table(
    path: Path, records: list[tuple[int, int, int]], show_utc: bool
) -> None:
    """Print decoded records as a human-readable table."""
    print(f"{path}: {len(records)} sample(s)")
    if show_utc:
        print(f"{'sample':>6}  {'seconds':>10}  {'UTC':<20}  {'ADC':>5}  {'mV':>5}")
        for index, (seconds, adc, millivolts) in enumerate(records):
            print(
                f"{index:6d}  {seconds:10d}  {utc_timestamp(seconds):<20}  "
                f"{adc:5d}  {millivolts:5d}"
            )
    else:
        print(f"{'sample':>6}  {'seconds':>10}  {'ADC':>5}  {'mV':>5}")
        for index, (seconds, adc, millivolts) in enumerate(records):
            print(f"{index:6d}  {seconds:10d}  {adc:5d}  {millivolts:5d}")


def display_csv(records: list[tuple[int, int, int]], show_utc: bool) -> None:
    """Print decoded records as CSV."""
    writer = csv.writer(sys.stdout, lineterminator="\n")
    header = ["sample", "seconds"]
    if show_utc:
        header.append("utc")
    header.extend(["adc", "millivolts"])
    writer.writerow(header)

    for index, (seconds, adc, millivolts) in enumerate(records):
        row: list[int | str] = [index, seconds]
        if show_utc:
            row.append(utc_timestamp(seconds))
        row.extend([adc, millivolts])
        writer.writerow(row)


def parse_args(argv: list[str] | None = None) -> argparse.Namespace:
    """Parse command-line arguments."""
    parser = argparse.ArgumentParser(
        description="Decode a gamma_*.dat file written by MosaicManager."
    )
    parser.add_argument("file", type=Path, help="downlinked gamma_*.dat file")
    parser.add_argument(
        "--utc",
        action="store_true",
        help="also interpret seconds as a Unix timestamp and display UTC",
    )
    parser.add_argument(
        "--csv", action="store_true", help="print CSV instead of a formatted table"
    )
    return parser.parse_args(argv)


def main(argv: list[str] | None = None) -> int:
    """Decode and display one MOSAIC sample file."""
    args = parse_args(argv)
    try:
        records = decode_file(args.file)
    except (OSError, ValueError) as error:
        print(f"error: {error}", file=sys.stderr)
        return 1

    if args.csv:
        display_csv(records, args.utc)
    else:
        display_table(args.file, records, args.utc)
    return 0


if __name__ == "__main__":
    sys.exit(main())

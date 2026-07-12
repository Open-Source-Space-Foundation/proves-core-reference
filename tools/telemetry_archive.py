#!/usr/bin/env python3
"""Validate and extract packetized telemetry archive segments."""

from __future__ import annotations

import argparse
import binascii
import json
import struct
from dataclasses import dataclass
from pathlib import Path

MAGIC = b"TLMARCH1"
HEADER = struct.Struct(">8sHH")
RECORD_HEADER = struct.Struct(">HI")
CRC = struct.Struct(">I")


@dataclass(frozen=True)
class Record:
    sequence: int
    packet: bytes


def read_records(path: Path) -> tuple[list[Record], bool]:
    """Return all valid records and whether an incomplete tail was ignored."""
    data = path.read_bytes()
    if len(data) < HEADER.size:
        raise ValueError("archive is shorter than its header")
    magic, version, _flags = HEADER.unpack_from(data)
    if magic != MAGIC:
        raise ValueError(f"invalid archive magic {magic!r}")
    if version != 1:
        raise ValueError(f"unsupported archive version {version}")

    records: list[Record] = []
    offset = HEADER.size
    truncated = False
    while offset < len(data):
        if len(data) - offset < RECORD_HEADER.size:
            truncated = True
            break
        packet_size, sequence = RECORD_HEADER.unpack_from(data, offset)
        end = offset + RECORD_HEADER.size + packet_size + CRC.size
        if end > len(data):
            truncated = True
            break
        packet_start = offset + RECORD_HEADER.size
        packet = data[packet_start : packet_start + packet_size]
        expected_crc = CRC.unpack_from(data, packet_start + packet_size)[0]
        actual_crc = binascii.crc32(data[offset + 2 : packet_start + packet_size])
        if actual_crc != expected_crc:
            raise ValueError(
                f"record {sequence} CRC mismatch: {actual_crc:08x} != {expected_crc:08x}"
            )
        records.append(Record(sequence, packet))
        offset = end
    return records, truncated


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("archive", type=Path)
    parser.add_argument(
        "--output",
        type=Path,
        help="write newline-delimited sequence/packet JSON to this path",
    )
    args = parser.parse_args()

    try:
        records, truncated = read_records(args.archive)
    except (OSError, ValueError) as error:
        parser.error(str(error))

    if args.output:
        with args.output.open("w", encoding="utf-8") as stream:
            for record in records:
                stream.write(
                    json.dumps(
                        {"sequence": record.sequence, "packet_hex": record.packet.hex()}
                    )
                    + "\n"
                )
    print(
        f"{args.archive}: {len(records)} valid records"
        + ("; ignored incomplete tail" if truncated else "")
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

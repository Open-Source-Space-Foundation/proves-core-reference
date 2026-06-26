#!/usr/bin/env python3
"""
gen_patch_seq.py – Saratoga-style hole detection for a received file segment.

Diffs a downloaded segment file against the ground-truth reference to find byte
ranges that were not received (zero-filled by the GDS at those offsets).  Emits
an F' command sequence (.seq text) containing one Cancel followed by a
SendPartial for each hole.  Uplink the compiled binary to the flight, then run
it with cmdSeq.CS_RUN – the flight autonomously retransmits only the missing
ranges without any per-hole round-trip from the ground.

Usage:
    gen_patch_seq.py --dl DL_FILE --ref REF_FILE \\
                     --flight-src FLIGHT_PATH --dest-name DEST_NAME \\
                     --seg-offset SEG_OFFSET --seg-len SEG_LEN \\
                     --out SEQ_FILE [--merge-gap BYTES]

Prints a single line "<n_holes> <total_seq_secs>" to stdout.
Exit 0 always; caller checks n_holes.
"""

import argparse
from pathlib import Path


def find_holes(dl_segment: bytes, ref_segment: bytes, merge_gap: int = 256):
    """Return (offset_in_segment, length) pairs for byte ranges where dl != ref.

    Ranges within merge_gap bytes of each other are merged into one request to
    reduce the number of SendPartial commands in the sequence.
    """
    n = len(ref_segment)
    holes: list[tuple[int, int]] = []
    in_hole = False
    hole_start = 0

    for i in range(n):
        dl_byte = dl_segment[i] if i < len(dl_segment) else 0
        differs = dl_byte != ref_segment[i]
        if differs and not in_hole:
            hole_start = i
            in_hole = True
        elif not differs and in_hole:
            holes.append((hole_start, i - hole_start))
            in_hole = False
    if in_hole:
        holes.append((hole_start, n - hole_start))

    if not holes or merge_gap <= 0:
        return holes

    merged: list[list[int]] = [[holes[0][0], holes[0][1]]]
    for start, length in holes[1:]:
        prev = merged[-1]
        if start <= prev[0] + prev[1] + merge_gap:
            prev[1] = (start + length) - prev[0]
        else:
            merged.append([start, length])
    return [(s, ln) for s, ln in merged]


def hold_secs(length: int) -> int:
    """Conservative per-hole wait in the sequence (seconds).

    Covers: START packet + ceil(length/chunk) DATA packets + END packet +
    fileDownlink COOLDOWN + sequencer dispatch overhead.  Uses a floor of 20s
    so tiny holes (single dropped packet) still get adequate time.
    """
    # ~200 B/s effective for short ranges (SF8 overhead dominates small payloads)
    return max(20, (length // 200 + 1) * 3 + 15)


def format_rel(secs: int) -> str:
    h, rem = divmod(secs, 3600)
    m, s = divmod(rem, 60)
    return f"R{h:02d}:{m:02d}:{s:02d}"


def main() -> None:
    ap = argparse.ArgumentParser(
        description="Generate an F' patch sequence for a CRC-mismatch segment."
    )
    ap.add_argument("--dl", required=True, help="Downloaded segment file path")
    ap.add_argument("--ref", required=True, help="Ground-truth reference file path")
    ap.add_argument(
        "--flight-src", required=True, help="Flight FS source path (e.g. //tp_asm.bin)"
    )
    ap.add_argument(
        "--dest-name",
        required=True,
        help="GDS destination filename (e.g. tp_asm_dl_1.bin)",
    )
    ap.add_argument(
        "--seg-offset",
        type=int,
        required=True,
        help="Byte offset of segment in the reference file",
    )
    ap.add_argument(
        "--seg-len", type=int, required=True, help="Length of segment in bytes"
    )
    ap.add_argument("--out", required=True, help="Output .seq text file path")
    ap.add_argument(
        "--merge-gap",
        type=int,
        default=256,
        help="Merge holes within this many bytes (default 256)",
    )
    ap.add_argument(
        "--cancel-delay",
        type=int,
        default=3,
        help="Seconds between Cancel and first SendPartial (default 3)",
    )
    args = ap.parse_args()

    ref_data = Path(args.ref).read_bytes()
    dl_data = Path(args.dl).read_bytes()

    ref_seg = ref_data[args.seg_offset : args.seg_offset + args.seg_len]
    dl_seg = dl_data[args.seg_offset : args.seg_offset + args.seg_len]

    holes = find_holes(dl_seg, ref_seg, args.merge_gap)

    if not holes:
        print("0 0")
        return

    lines = [
        f"; patch sequence: {args.dest_name} seg offset={args.seg_offset} len={args.seg_len}",
        f"; {len(holes)} hole(s): "
        + ", ".join(f"{args.seg_offset + s}+{ln}" for s, ln in holes),
        "",
        "R00:00:00 FileHandling.fileDownlink.Cancel",
    ]

    total_secs = args.cancel_delay
    prev_len = 0
    for idx, (seg_start, seg_len) in enumerate(holes):
        abs_offset = args.seg_offset + seg_start
        delay = args.cancel_delay if idx == 0 else hold_secs(prev_len)
        if idx > 0:
            total_secs += hold_secs(prev_len)
        lines.append(
            f"{format_rel(delay)} FileHandling.fileDownlink.SendPartial,"
            f' "{args.flight_src}", "{args.dest_name}", {abs_offset}, {seg_len}'
        )
        prev_len = seg_len

    # Time for the last SendPartial to complete (not a sequence line, but ground must wait)
    total_secs += hold_secs(prev_len)

    Path(args.out).write_text("\n".join(lines) + "\n")
    print(f"{len(holes)} {total_secs}")


if __name__ == "__main__":
    main()

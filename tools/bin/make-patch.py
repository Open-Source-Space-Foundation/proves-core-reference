#!/usr/bin/env python3
"""Build a PROVES delta patch that reconstructs a new image from one already on board.

A full signed image is ~727 KB, which at the ground station's file uplink pacing
(``file-uplink-chunk-size`` / ``file-uplink-cooldown`` in fprime-gds.yml, ~510 B/s) takes
roughly 24 minutes of contact. That does not fit a pass. A delta against an image the
spacecraft already holds is a small fraction of that.

The container written here is consumed by Components::PatchApplier on the flight side. It
carries the classic bsdiff instruction streams:

    magic      8 bytes  "PRVSPTCH"
    version    1 byte
    codec      1 byte   0 = none
    new_size   4 bytes  little endian, size of the reconstructed image
    ref_size   4 bytes  little endian, size of the reference the patch was built against
    ref_crc32  4 bytes  little endian, CRC32 of that reference
    ctrl_size  4 bytes  little endian
    diff_size  4 bytes  little endian
    extra_size 4 bytes  little endian
    control    ctrl_size bytes   records of three little endian int32: copy, extra, seek
    diff       diff_size bytes
    extra      extra_size bytes

The reference size and CRC are carried so the spacecraft can prove it is patching against the
image the ground actually diffed. Applying a patch to the wrong reference yields a plausible
but corrupt image that would then be flashed and booted, so this is checked before any work.

NOTE ON SIZE: codec 0 stores the streams verbatim, which makes the patch about the size of
the image itself and so of no uplink value. The streams are ~84% zero bytes and compress
extremely well (measured on real consecutive builds: ~60 KB with DEFLATE, ~47 KB with LZMA,
~97 KB with heatshrink), but decoding one on the flight side needs a decompressor that this
Zephyr workspace does not currently ship. Selecting that dependency is a project decision;
until it is made, this tool refuses to emit a patch unless --allow-uncompressed is passed,
so that nobody mistakes an uncompressed patch for something worth uplinking.
"""

import argparse
import bz2
import struct
import sys
import zlib
from pathlib import Path

MAGIC = b"PRVSPTCH"
FORMAT_VERSION = 1
COMPRESSION_NONE = 0

# Ground station file uplink rate implied by fprime-gds.yml, used for the size report
UPLINK_BYTES_PER_SECOND = 204 / 0.400


def parse_bsdiff40(patch: bytes) -> tuple[bytes, bytes, bytes, int]:
    """Split a classic BSDIFF40 patch into its three decompressed streams.

    bsdiff4 emits a well defined container: the magic, three 64 bit lengths, then the
    control, diff, and extra streams each bzip2 compressed. Re-encoding those streams into
    the PROVES container lets the proven diff algorithm do the hard part while keeping the
    on-orbit decoder trivial.

    Args:
        patch: a BSDIFF40 patch as produced by bsdiff4.diff

    Returns:
        control, diff, extra streams and the size of the reconstructed image
    """
    if patch[:8] != b"BSDIFF40":
        raise ValueError("not a BSDIFF40 patch")
    control_len = struct.unpack("<q", patch[8:16])[0]
    diff_len = struct.unpack("<q", patch[16:24])[0]
    new_size = struct.unpack("<q", patch[24:32])[0]

    cursor = 32
    control = bz2.decompress(patch[cursor : cursor + control_len])
    cursor += control_len
    diff = bz2.decompress(patch[cursor : cursor + diff_len])
    cursor += diff_len
    extra = bz2.decompress(patch[cursor:])
    return control, diff, extra, new_size


def transcode_control(control: bytes) -> bytes:
    """Re-encode bsdiff control records into fixed width little endian int32 triples.

    bsdiff stores offsets in a sign-magnitude 64 bit form. Flight images are bounded by the
    1 MB slot, so 32 bit records are ample and are far simpler to decode on the spacecraft.

    Args:
        control: the raw bsdiff control stream

    Returns:
        the re-encoded control stream

    Raises:
        ValueError: if a value does not fit in an int32, which would mean an image far
            larger than a slot
    """
    if len(control) % 24 != 0:
        raise ValueError("control stream is not a whole number of records")

    out = bytearray()
    for offset in range(0, len(control), 24):
        values = []
        for field in range(3):
            raw = struct.unpack(
                "<Q", control[offset + field * 8 : offset + field * 8 + 8]
            )[0]
            # bsdiff sign-magnitude: the high bit marks a negative value
            if raw & 0x8000000000000000:
                value = -(raw & 0x7FFFFFFFFFFFFFFF)
            else:
                value = raw
            if not (-(2**31) <= value < 2**31):
                raise ValueError(f"control value {value} does not fit in an int32")
            values.append(value)
        out += struct.pack("<iii", *values)
    return bytes(out)


def build_container(
    control: bytes, diff: bytes, extra: bytes, new_size: int, reference: bytes
) -> bytes:
    """Assemble the PROVES patch container.

    Args:
        control: re-encoded control stream
        diff: bsdiff difference stream
        extra: bsdiff literal stream
        new_size: size of the reconstructed image
        reference: the reference image, whose size and CRC bind the patch to it

    Returns:
        the complete container
    """
    header = MAGIC + struct.pack(
        "<BBIIIIII",
        FORMAT_VERSION,
        COMPRESSION_NONE,
        new_size,
        len(reference),
        crc32_fprime(reference),
        len(control),
        len(diff),
        len(extra),
    )
    return header + control + diff + extra


def crc32_fprime(data: bytes) -> int:
    """CRC32 in the form F Prime's Os::File::calculateCrc reports.

    F Prime accumulates with lib_crc's update_crc_32 from an initial 0xFFFFFFFF and does not
    invert the result, which is the complement of what zlib returns. tools/bin/calculate-crc.py
    does the same conversion; the two must agree or an uplinked image is rejected on board.

    Args:
        data: bytes to checksum

    Returns:
        the CRC32 as an unsigned 32 bit value
    """
    return ~zlib.crc32(data) & 0xFFFFFFFF


def parse_args() -> argparse.Namespace:
    """Parse command line arguments.

    Returns:
        the parsed arguments
    """
    parser = argparse.ArgumentParser(
        description="Build a PROVES delta patch from a reference image to a target image"
    )
    parser.add_argument("reference", type=Path, help="Image already on the spacecraft")
    parser.add_argument(
        "target", type=Path, help="Image to reconstruct on the spacecraft"
    )
    parser.add_argument(
        "-o", "--output", type=Path, required=True, help="Patch file to write"
    )
    parser.add_argument(
        "--allow-uncompressed",
        action="store_true",
        help="Emit the patch even though no codec is selected and it will be impractically large",
    )
    return parser.parse_args()


def main() -> int:
    """Entry point.

    Returns:
        process exit status
    """
    args = parse_args()
    try:
        import bsdiff4
    except ImportError:
        print(
            "bsdiff4 is required: pip install bsdiff4",
            file=sys.stderr,
        )
        return 1

    for path in (args.reference, args.target):
        if not path.is_file():
            print(f"no such file: {path}", file=sys.stderr)
            return 1

    reference = args.reference.read_bytes()
    target = args.target.read_bytes()

    control, diff, extra, new_size = parse_bsdiff40(bsdiff4.diff(reference, target))
    if new_size != len(target):
        raise ValueError("bsdiff reported a size that does not match the target image")
    container = build_container(
        transcode_control(control), diff, extra, new_size, reference
    )

    minutes = len(container) / UPLINK_BYTES_PER_SECOND / 60
    print(
        f"reference    {args.reference}  {len(reference)} bytes  CRC32 0x{crc32_fprime(reference):08x}"
    )
    print(f"target       {args.target}  {len(target)} bytes")
    print(f"patch        {len(container)} bytes  (~{minutes:.1f} min to uplink)")
    print(f"target CRC32 0x{crc32_fprime(target):08x}")

    if not args.allow_uncompressed:
        print(
            "\nrefusing to write: no compression codec is selected, so this patch is about the\n"
            "size of the image itself and is not worth uplinking. Pass --allow-uncompressed to\n"
            "write it anyway, for testing the container and the flight side applier.",
            file=sys.stderr,
        )
        return 2

    args.output.write_bytes(container)
    print(f"wrote        {args.output}")
    return 0


if __name__ == "__main__":
    sys.exit(main())

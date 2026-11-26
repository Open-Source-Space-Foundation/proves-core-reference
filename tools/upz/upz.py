#!/usr/bin/env python3
"""UPZ: compress a file for uplink so FileExpander can rebuild it on board.

Ground side of the compressed-uplink prototype. A Thumb-2 branch filter (BL and
B.W offsets made absolute, per 256 B block) is followed by raw LZMA
(lc=0 lp=1 pb=1). The stream is cut into chunks that each start from a fresh
LZMA state but are primed with every earlier byte, so the board can resume at
any chunk without saved coder state. See
PROVESFlightControllerReference/Components/FileExpander/UpzExpander.hpp for the
container layout.

Needs liblzma (xz-utils), loaded with ctypes: Python's lzma module cannot set a
preset dictionary.

    upz.py compress IN OUT [--no-filter] [--chunk-log2 16] [--dict 1048576]
    upz.py decompress IN OUT      # reference decoder (liblzma), for checks
    upz.py info IN
"""

import argparse
import ctypes
import ctypes.util
import struct
import sys
import zlib

MAGIC = b"UPZ1"
VERSION = 1
HEADER_FIXED = struct.Struct("<4sBBBBBBHIIIII")
BLOCK = 256
MAX_CHUNK_COUNT = 1024  # Upz::MAX_CHUNK_COUNT
LZMA_FILTER_LZMA1 = 0x4000000000000001
LZMA_VLI_UNKNOWN = 0xFFFFFFFFFFFFFFFF
LZMA_PRESET_EXTREME = 0x80000000
LZMA_OK, LZMA_STREAM_END = 0, 1
LZMA_RUN, LZMA_FINISH = 0, 3


# --------------------------------------------------------------------------
# Thumb-2 branch filter (mirrors Upz::thumbFilter)
# --------------------------------------------------------------------------
def thumb_filter_block(block: bytes, base: int, encode: bool) -> bytes:
    """Apply or undo the branch filter on one block whose first byte is at offset ``base``."""
    b = bytearray(block)
    i = 0
    n = len(b)
    while i + 4 <= n:
        h1 = b[i] | (b[i + 1] << 8)
        h2 = b[i + 2] | (b[i + 3] << 8)
        is_bl = (h2 & 0xD000) == 0xD000
        is_bw = (h2 & 0xD000) == 0x9000
        if (h1 & 0xF800) != 0xF000 or not (is_bl or is_bw):
            i += 2
            continue
        s = (h1 >> 10) & 1
        i1 = (~(((h2 >> 13) & 1) ^ s)) & 1
        i2 = (~(((h2 >> 11) & 1) ^ s)) & 1
        off = (
            (s << 24)
            | (i1 << 23)
            | (i2 << 22)
            | ((h1 & 0x3FF) << 12)
            | ((h2 & 0x7FF) << 1)
        )
        pc = base + i + 4
        t = ((off + pc) if encode else (off - pc)) & 0x1FFFFFF
        s2 = (t >> 24) & 1
        j1 = (~(((t >> 23) & 1) ^ s2)) & 1
        j2 = (~(((t >> 22) & 1) ^ s2)) & 1
        n1 = 0xF000 | (s2 << 10) | ((t >> 12) & 0x3FF)
        n2 = (
            (0xD000 if is_bl else 0x9000) | (j1 << 13) | (j2 << 11) | ((t >> 1) & 0x7FF)
        )
        b[i : i + 4] = struct.pack("<HH", n1, n2)
        i += 4
    return bytes(b)


def thumb_filter(data: bytes, encode: bool) -> bytes:
    """Apply or undo the branch filter block by block."""
    return b"".join(
        thumb_filter_block(data[o : o + BLOCK], o, encode)
        for o in range(0, len(data), BLOCK)
    )


# --------------------------------------------------------------------------
# liblzma via ctypes
# --------------------------------------------------------------------------
class _Opt(ctypes.Structure):
    """liblzma lzma_options_lzma."""

    _fields_ = [
        ("dict_size", ctypes.c_uint32),
        ("preset_dict", ctypes.c_void_p),
        ("preset_dict_size", ctypes.c_uint32),
        ("lc", ctypes.c_uint32),
        ("lp", ctypes.c_uint32),
        ("pb", ctypes.c_uint32),
        ("mode", ctypes.c_int),
        ("nice_len", ctypes.c_uint32),
        ("mf", ctypes.c_int),
        ("depth", ctypes.c_uint32),
        ("ext_flags", ctypes.c_uint32),
        ("ext_size_low", ctypes.c_uint32),
        ("ext_size_high", ctypes.c_uint32),
        ("reserved_int4", ctypes.c_uint32),
        ("reserved_int5", ctypes.c_uint32),
        ("reserved_int6", ctypes.c_uint32),
        ("reserved_int7", ctypes.c_uint32),
        ("reserved_int8", ctypes.c_uint32),
        ("reserved_enum1", ctypes.c_int),
        ("reserved_enum2", ctypes.c_int),
        ("reserved_enum3", ctypes.c_int),
        ("reserved_enum4", ctypes.c_int),
        ("reserved_ptr1", ctypes.c_void_p),
        ("reserved_ptr2", ctypes.c_void_p),
    ]


class _Filter(ctypes.Structure):
    """liblzma lzma_filter."""

    _fields_ = [("id", ctypes.c_uint64), ("options", ctypes.c_void_p)]


class _Stream(ctypes.Structure):
    """liblzma lzma_stream."""

    _fields_ = [
        ("next_in", ctypes.c_void_p),
        ("avail_in", ctypes.c_size_t),
        ("total_in", ctypes.c_uint64),
        ("next_out", ctypes.c_void_p),
        ("avail_out", ctypes.c_size_t),
        ("total_out", ctypes.c_uint64),
        ("allocator", ctypes.c_void_p),
        ("internal", ctypes.c_void_p),
        ("reserved_ptr1", ctypes.c_void_p),
        ("reserved_ptr2", ctypes.c_void_p),
        ("reserved_ptr3", ctypes.c_void_p),
        ("reserved_ptr4", ctypes.c_void_p),
        ("seek_pos", ctypes.c_uint64),
        ("reserved_int2", ctypes.c_uint64),
        ("reserved_int3", ctypes.c_size_t),
        ("reserved_int4", ctypes.c_size_t),
        ("reserved_enum1", ctypes.c_int),
        ("reserved_enum2", ctypes.c_int),
    ]


def _liblzma():
    """Load liblzma or exit with a hint."""
    name = ctypes.util.find_library("lzma") or "liblzma.so.5"
    try:
        return ctypes.CDLL(name)
    except OSError as exc:  # pragma: no cover - environment problem
        sys.exit(f"upz: liblzma not found ({exc}); install xz-utils / liblzma")


_L = _liblzma()


def _lzma1(
    encode: bool,
    payload: bytes,
    preset: bytes,
    dict_size: int,
    lc: int,
    lp: int,
    pb: int,
    out_cap: int,
) -> bytes:
    opt = _Opt()
    if (
        _L.lzma_lzma_preset(ctypes.byref(opt), ctypes.c_uint32(9 | LZMA_PRESET_EXTREME))
        != 0
    ):
        raise RuntimeError("lzma_lzma_preset failed")
    opt.dict_size, opt.lc, opt.lp, opt.pb, opt.nice_len = dict_size, lc, lp, pb, 273
    keep = None
    if preset:
        keep = ctypes.create_string_buffer(preset, len(preset))
        opt.preset_dict = ctypes.addressof(keep)
        opt.preset_dict_size = len(preset)
    filters = (_Filter * 2)()
    filters[0].id = LZMA_FILTER_LZMA1
    filters[0].options = ctypes.cast(ctypes.byref(opt), ctypes.c_void_p)
    filters[1].id = LZMA_VLI_UNKNOWN
    strm = _Stream()
    init = _L.lzma_raw_encoder if encode else _L.lzma_raw_decoder
    if init(ctypes.byref(strm), filters) != LZMA_OK:
        raise RuntimeError("liblzma raw coder init failed")
    inbuf = ctypes.create_string_buffer(payload, len(payload))
    out = ctypes.create_string_buffer(out_cap)
    strm.next_in, strm.avail_in = ctypes.addressof(inbuf), len(payload)
    strm.next_out, strm.avail_out = ctypes.addressof(out), out_cap
    try:
        while True:
            ret = _L.lzma_code(ctypes.byref(strm), LZMA_FINISH)
            if ret == LZMA_STREAM_END:
                break
            if ret != LZMA_OK or (strm.avail_out == 0 and not encode):
                if not encode and strm.total_out == out_cap:
                    break  # known size reached; trailing end marker not needed
                raise RuntimeError(f"liblzma returned {ret}")
        return out.raw[: strm.total_out]
    finally:
        _L.lzma_end(ctypes.byref(strm))
        del keep


# --------------------------------------------------------------------------
# Container
# --------------------------------------------------------------------------
def compress(
    raw: bytes,
    use_filter: bool = True,
    chunk_log2: int = 16,
    dict_size: int = 1 << 20,
    lc: int = 0,
    lp: int = 1,
    pb: int = 1,
) -> bytes:
    if lc + lp > 1 or pb > 4 or not 12 <= chunk_log2 <= 20:
        raise ValueError("unsupported parameters (lc+lp<=1, pb<=4, 12<=chunk_log2<=20)")
    chunk = 1 << chunk_log2
    if len(raw) >= 1 << 32 or -(-len(raw) // chunk) > MAX_CHUNK_COUNT:
        raise ValueError(
            f"too large: the board takes at most {MAX_CHUNK_COUNT} chunks of {chunk} bytes"
        )
    filtered = thumb_filter(raw, True) if use_filter else raw
    coded = []
    table = []
    end = 0
    for start in range(0, len(raw), chunk):
        piece = filtered[start : start + chunk]
        preset = filtered[max(0, start - dict_size) : start]
        c = _lzma1(True, piece, preset, dict_size, lc, lp, pb, 2 * len(piece) + 1024)
        coded.append(c)
        end += len(c)
        table.append((end, zlib.crc32(raw[start : start + chunk])))
    header = HEADER_FIXED.pack(
        MAGIC,
        VERSION,
        1 if use_filter else 0,
        lc,
        lp,
        pb,
        chunk_log2,
        0,
        dict_size,
        len(raw),
        zlib.crc32(raw),
        len(table),
        end,
    )
    header += b"".join(struct.pack("<II", e, c) for e, c in table)
    header += struct.pack("<I", zlib.crc32(header))
    return header + b"".join(coded)


def parse(blob: bytes) -> dict:
    """Parse and check a UPZ header; return its fields and chunk table."""
    fields = HEADER_FIXED.unpack_from(blob, 0)
    (
        magic,
        version,
        filt,
        lc,
        lp,
        pb,
        chunk_log2,
        _,
        dict_size,
        raw_size,
        raw_crc,
        count,
        coded_size,
    ) = fields
    if magic != MAGIC or version != VERSION:
        raise ValueError("not a UPZ1 file")
    table = [
        struct.unpack_from("<II", blob, HEADER_FIXED.size + 8 * i) for i in range(count)
    ]
    data_start = HEADER_FIXED.size + 8 * count + 4
    (hcrc,) = struct.unpack_from("<I", blob, data_start - 4)
    if zlib.crc32(blob[: data_start - 4]) != hcrc:
        raise ValueError("header CRC mismatch")
    if len(blob) != data_start + coded_size:
        raise ValueError("file size does not match header")
    return dict(
        filter=filt,
        lc=lc,
        lp=lp,
        pb=pb,
        chunk_log2=chunk_log2,
        dict_size=dict_size,
        raw_size=raw_size,
        raw_crc=raw_crc,
        table=table,
        data_start=data_start,
        coded_size=coded_size,
    )


def decompress(blob: bytes) -> bytes:
    """Reference decoder through liblzma; checks every chunk CRC and the whole-file CRC."""
    h = parse(blob)
    chunk = 1 << h["chunk_log2"]
    out = bytearray()
    prev_end = 0
    for i, (end, _) in enumerate(h["table"]):
        n = min(chunk, h["raw_size"] - i * chunk)
        preset = bytes(out[max(0, len(out) - h["dict_size"]) :])
        payload = blob[h["data_start"] + prev_end : h["data_start"] + end]
        out += _lzma1(
            False, payload, preset, h["dict_size"], h["lc"], h["lp"], h["pb"], n
        )
        prev_end = end
    raw = thumb_filter(bytes(out), False) if h["filter"] == 1 else bytes(out)
    for i, (_, crc) in enumerate(h["table"]):
        if zlib.crc32(raw[i * chunk : (i + 1) * chunk]) != crc:
            raise ValueError(f"chunk {i} CRC mismatch")
    if zlib.crc32(raw) != h["raw_crc"]:
        raise ValueError("raw CRC mismatch")
    return raw


def main(argv=None) -> int:
    """Command-line entry point."""
    ap = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter
    )
    sub = ap.add_subparsers(dest="cmd", required=True)
    c = sub.add_parser("compress")
    c.add_argument("input")
    c.add_argument("output")
    c.add_argument(
        "--no-filter",
        action="store_true",
        help="skip the Thumb-2 filter (non-code files)",
    )
    c.add_argument("--chunk-log2", type=int, default=16)
    c.add_argument("--dict", type=int, default=1 << 20)
    d = sub.add_parser("decompress")
    d.add_argument("input")
    d.add_argument("output")
    i = sub.add_parser("info")
    i.add_argument("input")
    a = ap.parse_args(argv)
    if a.cmd == "compress":
        raw = open(a.input, "rb").read()
        blob = compress(raw, not a.no_filter, a.chunk_log2, a.dict)
        if decompress(blob) != raw:
            raise SystemExit("upz: round trip failed")
        open(a.output, "wb").write(blob)
        print(
            f"{len(raw)} -> {len(blob)} bytes (ratio {len(blob) / max(1, len(raw)):.3f}), "
            f"raw crc32 0x{zlib.crc32(raw):08x}"
        )
    elif a.cmd == "decompress":
        open(a.output, "wb").write(decompress(open(a.input, "rb").read()))
    else:
        h = parse(open(a.input, "rb").read())
        h.pop("table")
        print(" ".join(f"{k}={v}" for k, v in h.items()))
    return 0


if __name__ == "__main__":
    sys.exit(main())

"""Post-pass triage of ground-station byte captures.

Segments a per-byte capture CSV (timestamp,byte_index,hex_value,state — as
emitted by the instrumented adapter) into attributed packets and prints a
human-readable report to the terminal:

  * PROVES TM downlink — CRC-verified 248-byte frames at any offset, plus
    signature-based partials (sync word / frame-counter / idle-fill).
  * CCSDS TC uplink — structurally parsed transfer frames, tiered:
    own-uplink (CRC ok, SCID 68), foreign-uplink (CRC ok, other SCID),
    own-uplink-degraded (SCID 68 header parses but CRC fails).
  * HUCSat — exact beacon/telemetry signatures with a fuzzy "degraded"
    fallback for corrupted copies (relevant once promiscuous RX flies).
  * Watched commands — decoded from CRC-valid TC frames (verified verdicts,
    args included) plus a raw opcode sweep over unattributed bytes (partial
    verdicts). Opcodes resolve from the flown F´ dictionary at runtime.

Import use (from proves_adapter.py or anywhere):

    from pass_triage import triage
    triage("capture.csv")                 # prints report, returns it as str

CLI use:

    python pass_triage.py capture.csv [--dictionary DICT.json]
"""

import argparse
import csv
import json
import re
import sys
from datetime import datetime
from pathlib import Path

# ---------------------------------------------------------------------------
# Constants
# ---------------------------------------------------------------------------

TM_FRAME_LENGTH = 248  # TmFrameFixedSize — fixed CCSDS TM frame size
OWN_SCID = 68

# HUCSat signatures (post GRC 4-byte header strip).  Packets look like
#   <ctr> 00 00 00 01 4d <body>
# where body is the 6-byte callsign (beacon, 12 bytes total) or a record
# blob starting 00 00 00 a2 00 06 "HUCSat" and ending d5 05 4e 61 b8 f4.
HUCSAT_CALLSIGN = b"WP2XZJ"
HUCSAT_PREFIX = b"\x00\x00\x00\x01\x4d"
HUCSAT_LONG_START = b"\x00\x00\x00\xa2\x00\x06HUCSat"
HUCSAT_LONG_END = b"\xd5\x05\x4e\x61\xb8\xf4"
HUCSAT_BEACON_LEN = 12
HUCSAT_LONG_NOMINAL_LEN = 202
FUZZY_MAX_ERRORS = 2

# Watched commands (names resolved against the dictionary at load time).
WATCHLIST = [
    "CdhCore.cmdDisp.CMD_NO_OP",
    "ReferenceDeployment.lora.TRANSMIT",
    "ReferenceDeployment.downlinkDelay.DIVIDER_PRM_SET",
    "ReferenceDeployment.telemetryDelay.DIVIDER_PRM_SET",
]

# Fallback opcodes if the dictionary is unavailable (flown build, 2026-07).
FALLBACK_OPCODES = {
    "CdhCore.cmdDisp.CMD_NO_OP": 0x01000000,
    "ReferenceDeployment.lora.TRANSMIT": 0x1001F009,
    "ReferenceDeployment.downlinkDelay.DIVIDER_PRM_SET": 0x1001E000,
    "ReferenceDeployment.telemetryDelay.DIVIDER_PRM_SET": 0x10061000,
}
FALLBACK_TRANSMIT_STATES = {0: "ENABLED", 1: "DISABLED", 2: "DISABLING"}

DEFAULT_DICTIONARY = (
    Path(__file__).resolve().parents[2]
    / "build-artifacts/zephyr/fprime-zephyr-deployment/dict"
    / "ReferenceDeploymentTopologyDictionary.json"
)

# Auth wrapper inside a TC frame payload: SPI(2) + SeqNum(4) + SP + HMAC(16)
AUTH_HEADER_SIZE = 6
AUTH_HMAC_SIZE = 16
SP_HEADER_SIZE = 6

BURST_GAP_SECONDS = 5.0


# ---------------------------------------------------------------------------
# Small helpers
# ---------------------------------------------------------------------------


def crc16_ccitt(data: bytes) -> int:
    crc = 0xFFFF
    for byte in data:
        crc ^= byte << 8
        for _ in range(8):
            crc = ((crc << 1) ^ 0x1021 if crc & 0x8000 else crc << 1) & 0xFFFF
    return crc


def _hamming(a: bytes, b: bytes) -> int:
    if len(a) != len(b):
        return max(len(a), len(b))
    return sum(1 for x, y in zip(a, b) if x != y)


def load_capture(path):
    """Read the per-byte CSV, returning (data bytes, per-byte datetimes)."""
    data = bytearray()
    times = []
    with open(path, newline="") as fh:
        for row in csv.DictReader(fh):
            data.append(int(row["hex_value"], 16))
            times.append(datetime.strptime(row["timestamp"], "%Y-%m-%d %H:%M:%S"))
    return bytes(data), times


def read_bursts(times):
    """Byte offsets where the capture's read bursts begin (timestamp gaps)."""
    starts = [0]
    for i in range(1, len(times)):
        if (times[i] - times[i - 1]).total_seconds() > BURST_GAP_SECONDS:
            starts.append(i)
    return starts


def load_watchlist(dictionary_path):
    """Resolve watched command opcodes + TransmitState names from the dictionary.

    Returns (opcodes: name->opcode, transmit_states: value->name, source: str).
    """
    path = Path(dictionary_path) if dictionary_path else DEFAULT_DICTIONARY
    try:
        with open(path) as fh:
            dictionary = json.load(fh)
    except OSError:
        return dict(FALLBACK_OPCODES), dict(FALLBACK_TRANSMIT_STATES), "FALLBACK CONSTANTS (dictionary not found)"

    by_name = {c["name"]: c["opcode"] for c in dictionary.get("commands", [])}
    opcodes = {}
    for name in WATCHLIST:
        if name in by_name:
            opcodes[name] = by_name[name]
    transmit_states = dict(FALLBACK_TRANSMIT_STATES)
    for tdef in dictionary.get("typeDefinitions", []):
        if tdef.get("qualifiedName", "").endswith("TransmitState"):
            transmit_states = {
                c["value"]: c["name"] for c in tdef.get("enumeratedConstants", [])
            }
    return opcodes, transmit_states, str(path)


# ---------------------------------------------------------------------------
# Packet detectors — each returns (tag, length, detail) or None
# ---------------------------------------------------------------------------


def match_proves_tm(data, i):
    if i + TM_FRAME_LENGTH > len(data):
        return None
    frame = data[i : i + TM_FRAME_LENGTH]
    if crc16_ccitt(frame[:-2]) == int.from_bytes(frame[-2:], "big"):
        scid = ((frame[0] << 8) | frame[1]) >> 4 & 0x3FF
        return ("proves-tm", TM_FRAME_LENGTH, f"SCID={scid} vc_count={frame[3]}")
    return None


def parse_tc_header(data, i):
    """Parse a CCSDS TC transfer frame header at offset i.

    Returns (scid, vcid, total_len) if structurally plausible, else None.
    """
    if i + 7 > len(data):
        return None
    b0, b1, b2, b3 = data[i : i + 4]
    version = (b0 >> 6) & 0x3
    spare = (b0 >> 2) & 0x3
    if version != 0 or spare != 0:
        return None
    scid = ((b0 & 0x03) << 8) | b1
    vcid = (b2 >> 2) & 0x3F
    total_len = (((b2 & 0x03) << 8) | b3) + 1
    if not (8 <= total_len <= 256):
        return None
    return scid, vcid, total_len


def match_tc_frame(data, i):
    parsed = parse_tc_header(data, i)
    if parsed is None:
        return None
    scid, vcid, total_len = parsed
    if i + total_len > len(data):
        return None
    frame = data[i : i + total_len]
    crc_ok = crc16_ccitt(frame[:-2]) == int.from_bytes(frame[-2:], "big")
    if crc_ok:
        tag = "own-uplink" if scid == OWN_SCID else "foreign-uplink"
        return (tag, total_len, f"SCID={scid} VCID={vcid}")
    # Degraded tier: only claim garbled uplink for our own SCID, to keep
    # false positives out of arbitrary payload bytes.
    if scid == OWN_SCID:
        return ("own-uplink-degraded", total_len, f"SCID={scid} VCID={vcid} CRC FAIL")
    return None


def match_hucsat(data, i):
    prefix = data[i + 1 : i + 6]
    exact = prefix == HUCSAT_PREFIX
    fuzzy = not exact and _hamming(prefix, HUCSAT_PREFIX) <= FUZZY_MAX_ERRORS
    if not (exact or fuzzy):
        return None

    body = data[i + 6 : i + 12]
    body_exact = body == HUCSAT_CALLSIGN
    body_fuzzy = _hamming(body, HUCSAT_CALLSIGN) <= FUZZY_MAX_ERRORS
    if body_exact or body_fuzzy:
        degraded = fuzzy or not body_exact
        tag = "hucsat-beacon" + ("-degraded" if degraded else "")
        return (tag, HUCSAT_BEACON_LEN, f"ctr={data[i]:02x}")

    long_start = data[i + 6 : i + 6 + len(HUCSAT_LONG_START)]
    ls_exact = long_start == HUCSAT_LONG_START
    ls_fuzzy = _hamming(long_start, HUCSAT_LONG_START) <= FUZZY_MAX_ERRORS
    if ls_exact or ls_fuzzy:
        end = data.find(HUCSAT_LONG_END, i)
        degraded = fuzzy or not ls_exact
        if end != -1:
            length = end + len(HUCSAT_LONG_END) - i
            tag = "hucsat-telemetry" + ("-degraded" if degraded else "")
            return (tag, length, f"ctr={data[i]:02x}")
        length = min(HUCSAT_LONG_NOMINAL_LEN, len(data) - i)
        return ("hucsat-telemetry-truncated", length, f"ctr={data[i]:02x}")
    return None


def match_proves_tm_partial(data, i):
    """Signature-based partial-TM detection when the CRC sweep found nothing."""
    score = 0
    notes = []
    if data[i : i + 2] == b"\x04\x42":
        score += 1
        notes.append("sync")
        if i + 4 <= len(data) and data[i + 2] == data[i + 3]:
            score += 1
            notes.append("ctr-eq")
        if data[i + 4 : i + 6] == b"\x18\x00":
            score += 1
            notes.append("fhp")
    if data[i : i + 4] == b"\x07\xff\xc0\x00":
        score += 2
        notes.append("idle-hdr")
    run = re.match(b"\x44{6,}", data[i:])
    if run:
        score += 1
        notes.append(f"fill x{len(run.group())}")
    if score >= 2:
        length = min(TM_FRAME_LENGTH, len(data) - i)
        return ("proves-tm-partial", length, "+".join(notes))
    return None


DETECTORS = (match_proves_tm, match_tc_frame, match_hucsat, match_proves_tm_partial)


def segment(data, times):
    """Greedy single-pass segmentation into (offset, time, tag, length, detail)."""
    packets = []
    unknown_start = None
    i = 0
    while i < len(data):
        hit = None
        for detector in DETECTORS:
            hit = detector(data, i)
            if hit:
                break
        if hit:
            if unknown_start is not None:
                packets.append(
                    (unknown_start, times[unknown_start], "unknown", i - unknown_start, "")
                )
                unknown_start = None
            tag, length, detail = hit
            packets.append((i, times[i], tag, length, detail))
            i += length
        else:
            if unknown_start is None:
                unknown_start = i
            i += 1
    if unknown_start is not None:
        packets.append(
            (unknown_start, times[unknown_start], "unknown", len(data) - unknown_start, "")
        )
    return packets


# ---------------------------------------------------------------------------
# Command extraction
# ---------------------------------------------------------------------------


def decode_tc_commands(data, packets, opcodes, transmit_states):
    """Decode watched commands from CRC-valid TC frames.

    Returns a list of (name, arg_desc, offset, time, verified: bool).
    """
    found = []
    by_opcode = {v: k for k, v in opcodes.items()}
    for offset, when, tag, length, _ in packets:
        if tag not in ("own-uplink", "foreign-uplink"):
            continue
        payload = data[offset + 5 : offset + length - 2]
        # Auth wrapper (SPI + SeqNum + SP + HMAC) or a bare space packet.
        candidates = []
        if len(payload) >= AUTH_HEADER_SIZE + SP_HEADER_SIZE + AUTH_HMAC_SIZE:
            candidates.append(payload[AUTH_HEADER_SIZE : len(payload) - AUTH_HMAC_SIZE])
        candidates.append(payload)
        for sp in candidates:
            if len(sp) < SP_HEADER_SIZE + 6:
                continue
            sp_len = ((sp[4] << 8) | sp[5]) + 1 + SP_HEADER_SIZE
            if sp_len != len(sp):
                continue
            body = sp[SP_HEADER_SIZE:]
            opcode = int.from_bytes(body[2:6], "big")  # descriptor(2) + opcode(4)
            name = by_opcode.get(opcode)
            if name:
                found.append(
                    (name, _describe_args(name, body[6:], transmit_states), offset, when, True)
                )
            break
    return found


def _describe_args(name, arg_bytes, transmit_states):
    if not arg_bytes:
        return ""
    value = int.from_bytes(arg_bytes, "big")
    if name.endswith("lora.TRANSMIT"):
        return transmit_states.get(value, f"?{value}")
    return str(value)


def sweep_opcodes(data, packets, opcodes, transmit_states):
    """Raw opcode-pattern search over bytes not already attributed to a
    verified TC frame. Returns (name, arg_desc, offset, time, verified=False)."""
    covered = bytearray(len(data))
    for offset, _, tag, length, _ in packets:
        if tag in ("own-uplink", "foreign-uplink"):
            covered[offset : offset + length] = b"\x01" * length
    found = []
    for name, opcode in opcodes.items():
        needle = opcode.to_bytes(4, "big")
        for m in re.finditer(re.escape(needle), data):
            i = m.start()
            if any(covered[i : i + 4]):
                continue
            arg = ""
            if name.endswith("lora.TRANSMIT") and i + 4 < len(data):
                arg = transmit_states.get(data[i + 4], f"?{data[i + 4]}")
            found.append((name, arg, i, None, False))
    return found


# ---------------------------------------------------------------------------
# Report
# ---------------------------------------------------------------------------


def _fmt_time(t):
    return t.strftime("%H:%M:%S") if t else "--:--:--"


def build_report(path, data, times, packets, commands, opcodes, transmit_states, dict_source):
    lines = []
    out = lines.append
    bursts = read_bursts(times)

    out(f"Pass triage: {path}")
    out(
        f"Span {_fmt_time(times[0])} → {_fmt_time(times[-1])}   "
        f"({len(data)} bytes, {len(bursts)} read bursts)"
    )
    out(f"Dictionary: {dict_source}")
    out("")

    def total(prefix):
        return sum(l for _, _, t, l, _ in packets if t.startswith(prefix))

    def count(prefix):
        return sum(1 for _, _, t, _, _ in packets if t.startswith(prefix))

    out("VERDICT")
    tm_v = sum(1 for _, _, t, _, _ in packets if t == "proves-tm")
    tm_p = count("proves-tm-partial")
    if tm_v:
        out(f"  PROVES downlink : HEARD — {tm_v} verified frame(s), {tm_p} partial")
    elif tm_p:
        out(f"  PROVES downlink : POSSIBLE — 0 verified, {tm_p} partial signature hit(s)")
    else:
        out("  PROVES downlink : NOT HEARD (0 verified frames, 0 partial signatures)")

    own = count("own-uplink")
    foreign = count("foreign-uplink")
    uplink_bits = []
    if own:
        uplink_bits.append(f"{own} own-SCID frame(s)")
    if foreign:
        uplink_bits.append(f"{foreign} foreign frame(s)")
    out(f"  Uplink co-heard : {', '.join(uplink_bits) if uplink_bits else 'none detected'}")

    huc_b, huc_t = count("hucsat-beacon"), count("hucsat-telemetry")
    huc_bytes = total("hucsat")
    pct = 100 * huc_bytes // len(data) if data else 0
    out(
        f"  HUCSat          : {huc_b + huc_t} packet(s) "
        f"({huc_b} beacon, {huc_t} telemetry) — {huc_bytes} bytes ({pct}%)"
    )
    out(f"  Unknown         : {total('unknown')} bytes in {count('unknown')} run(s)")
    out("")

    out("WATCHED COMMANDS")
    for name in opcodes:
        hits = [c for c in commands if c[0] == name]
        if not hits:
            out(f"  {name:<52} not seen")
            continue
        for cmd_name, arg, offset, when, verified in hits:
            label = f"{cmd_name} {arg}".strip()
            status = "UPLINKED (verified)" if verified else "possible (opcode match only)"
            out(f"  {label:<52} {status} @byte {offset} {_fmt_time(when)}")
    out("")

    out("PACKETS")
    out(f"  {'offset':>6}  {'time':<8}  {'tag':<26} {'len':>4}  detail")
    for offset, when, tag, length, detail in packets:
        out(f"  {offset:>6}  {_fmt_time(when):<8}  {tag:<26} {length:>4}  {detail}")
    return "\n".join(lines)


# ---------------------------------------------------------------------------
# Entry points
# ---------------------------------------------------------------------------


def triage(filename, dictionary=None, quiet=False):
    """Analyze a pass capture CSV; print the report and return it as a string."""
    data, times = load_capture(filename)
    if not data:
        report = f"Pass triage: {filename}\n(empty capture)"
        if not quiet:
            print(report)
        return report
    opcodes, transmit_states, dict_source = load_watchlist(dictionary)
    packets = segment(data, times)
    commands = decode_tc_commands(data, packets, opcodes, transmit_states)
    commands += sweep_opcodes(data, packets, opcodes, transmit_states)
    report = build_report(
        filename, data, times, packets, commands, opcodes, transmit_states, dict_source
    )
    if not quiet:
        print(report)
    return report


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("capture", help="per-byte capture CSV from the adapter")
    parser.add_argument(
        "--dictionary",
        default=None,
        help=f"F´ dictionary JSON (default: {DEFAULT_DICTIONARY})",
    )
    args = parser.parse_args()
    triage(args.capture, dictionary=args.dictionary)


if __name__ == "__main__":
    sys.exit(main())

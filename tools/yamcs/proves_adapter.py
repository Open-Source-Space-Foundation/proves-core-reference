"""
PROVES YAMCS Adapter — Option B authentication bridge.

Routes TM frames from the spacecraft to YAMCS (via UDP) and TC frames from
YAMCS (via UDP) back to the spacecraft after wrapping them with the HMAC-SHA256
authentication header/trailer required by the FSW Authenticate component.

The TM side is promiscuous: it accepts frames from any CCSDS spacecraft ID
without an explicit allow-list.  Before forwarding to YAMCS the SCID/VCID
bytes are normalized to ``--spacecraft-id`` (default 68) and the frame CRC
is recomputed, so YAMCS's ``spacecraftId`` filter always passes regardless of
which satellite the GRC board heard.

Use Case 1 (local UART):
    python proves_adapter.py --mode serial --uart-device /dev/ttyUSB0

Use Case 2 (remote TCP, skeleton):
    python proves_adapter.py --mode tcp --tcp-host <gs-host> --tcp-port 5000 \
        --yamcs-host <server-ip>
"""

import argparse
import socket
import sys
import threading
from collections import Counter
from datetime import datetime
from pathlib import Path
import time

# Flush stdout on every print so diagnostic messages appear immediately even
# when the adapter is run as a background process or piped to a log file.
sys.stdout.reconfigure(line_buffering=True)

# Allow importing authenticate_plugin from the Framing package.  Must be set
# before the import below so authenticate_plugin.py (and fprime_gds) load OK.
sys.path.insert(0, str(Path(__file__).parents[2] / "Framing" / "src"))

# ---------------------------------------------------------------------------
# CRC16-CCITT (CCSDS standard: poly 0x1021, init 0xFFFF, no reflection)
# ---------------------------------------------------------------------------

data_path = Path(__file__).parent / "data"


def _format_timestamp(when: datetime | None = None) -> str:
    """Human-readable local timestamp: YYYY-MM-DD HH:MM:SS."""
    return (when or datetime.now()).strftime("%Y-%m-%d %H:%M:%S")


def _crc16_ccitt(data: bytes) -> int:
    crc = 0xFFFF
    for byte in data:
        crc ^= byte << 8
        for _ in range(8):
            crc = ((crc << 1) ^ 0x1021 if crc & 0x8000 else crc << 1) & 0xFFFF
    return crc


def _hex_dump_lines(data: bytes, base_offset: int = 0) -> list[str]:
    """Format *data* as 16-byte hex rows (format B)."""
    lines = []
    for offset in range(0, len(data), 16):
        chunk = data[offset : offset + 16]
        hex_part = " ".join(f"{byte_val:02x}" for byte_val in chunk)
        lines.append(f"  {base_offset + offset:04x}: {hex_part}")
    return lines


def _write_frame_hex_block(hex_file, header: str, frame: bytes) -> None:
    """Append one format-C frame block (header + 16-byte hex rows)."""
    hex_file.write(header + "\n")
    for line in _hex_dump_lines(frame):
        hex_file.write(line + "\n")
    hex_file.write("\n")
    hex_file.flush()


from authenticate_plugin import (  # noqa: E402
    AuthenticateFramer,
    get_default_auth_key_from_header,
)
from fprime_gds.common.communication.ccsds.space_data_link import (  # noqa: E402
    SpaceDataLinkFramerDeframer,
)

# ---------------------------------------------------------------------------
# TM path helpers
# ---------------------------------------------------------------------------


def _normalize_tm_scid(frame: bytes, spacecraft_id: int, vc_id: int) -> bytes:
    """Overwrite bytes 0-1 of a CCSDS TM frame with the target SCID/VCID and recompute CRC.

    CCSDS TM primary header word0 (bytes 0-1):
      bits 15-14: version=0
      bits 13-4:  spacecraft_id (10 bits)
      bits 3-1:   vc_id (3 bits)
      bit 0:      OCF flag = 0
    """
    word0 = (spacecraft_id << 4) | (vc_id << 1)
    out = bytearray(frame)
    out[0] = (word0 >> 8) & 0xFF
    out[1] = word0 & 0xFF
    crc = _crc16_ccitt(bytes(out[:-2]))
    out[-2] = (crc >> 8) & 0xFF
    out[-1] = crc & 0xFF
    return bytes(out)


def _forward_tm_serial(
    ser,
    tm_sock,
    yamcs_host: str,
    tm_port: int,
    frame_length: int,
    spacecraft_ids: list[int] | None = None,
    vc_id: int = 1,
    stream_csv: Path | None = None,
    stream_hex: Path | None = None,
    packets_hex: Path | None = None,
    dropped_hex: Path | None = None,
):
    """Read fixed-length TM frames from serial and forward to YAMCS via UDP.

    Accepts frames from *any* spacecraft ID using a CRC-16/CCITT-only HUNT/LOCK
    state machine — no sync-header allow-list is required.  Before forwarding,
    frames whose SCID doesn't match ``spacecraft_ids[0]`` are normalized
    (bytes 0-1 patched, CRC recomputed) so YAMCS's ``spacecraftId`` filter
    always passes.  Per-SCID counters track how many frames arrived from each
    satellite seen during the session.

    Every UART byte is logged to ``stream.hex`` on arrival and classified in
    ``stream.csv`` when consumed (``frame``, ``hunt``, or ``locked_discard``).
    Valid frames and CRC-failed windows are also written to ``packets.hex`` and
    ``dropped.hex`` respectively.

      HUNT — slide 1 byte at a time until a CRC-valid frame is found.
      LOCK — once in sync, take the next frame exactly ``frame_length`` bytes
             later; fall back to HUNT on CRC failure.
    """
    if spacecraft_ids is None:
        spacecraft_ids = [68]

    primary_scid = spacecraft_ids[0]
    print(f"[TM] serial → UDP {yamcs_host}:{tm_port}  (frame_length={frame_length})")
    print(
        f"[TM] Promiscuous: accepting any SCID, normalizing to SCID={primary_scid} VCID={vc_id}"
    )

    buf = bytearray()
    frames_sent_by_scid: Counter[int] = Counter()
    last_vc_count: dict[int, int] = {}
    vc_frame_gaps = 0
    discarded_bytes = 0
    locked = False
    stats_time = time.monotonic()
    session_byte_index = 0
    stream_hex_appended = 0
    packet_count = 0
    dropped_count = 0
    stream_hex_tail = bytearray()
    stream_csv_file = stream_csv.open("a") if stream_csv is not None else None
    stream_hex_file = stream_hex.open("a") if stream_hex is not None else None
    packets_hex_file = packets_hex.open("a") if packets_hex is not None else None
    dropped_hex_file = dropped_hex.open("a") if dropped_hex is not None else None

    def _buf_base_index() -> int:
        return session_byte_index - len(buf)

    def _append_stream_hex(byte_vals: bytes) -> None:
        nonlocal stream_hex_tail, stream_hex_appended
        if not byte_vals or stream_hex_file is None:
            return
        stream_hex_tail.extend(byte_vals)
        while len(stream_hex_tail) >= 16:
            row_offset = stream_hex_appended
            row = bytes(stream_hex_tail[:16])
            del stream_hex_tail[:16]
            stream_hex_appended += 16
            hex_part = " ".join(f"{byte_val:02x}" for byte_val in row)
            stream_hex_file.write(f"  {row_offset:04x}: {hex_part}\n")
        stream_hex_file.flush()

    def _log_classified_byte(byte_index: int, byte_val: int, state: str) -> None:
        ts = _format_timestamp()
        print(
            f"[TM] byte | {ts} | idx={byte_index:6d} | 0x{byte_val:02x} | {state}",
            flush=True,
        )
        if stream_csv_file is not None:
            stream_csv_file.write(
                f"{ts},{byte_index},0x{byte_val:02x},{state}\n"
            )

    def _log_classified_frame(raw_frame: bytes, base_index: int) -> None:
        nonlocal packet_count
        ts = _format_timestamp()
        frame_scid = ((raw_frame[0] << 8) | raw_frame[1]) >> 4 & 0x3FF
        vc_count = raw_frame[3]
        for offset, byte_val in enumerate(raw_frame):
            byte_index = base_index + offset
            print(
                f"[TM] byte | {ts} | idx={byte_index:6d} | 0x{byte_val:02x} | frame",
                flush=True,
            )
            if stream_csv_file is not None:
                stream_csv_file.write(
                    f"{ts},{byte_index},0x{byte_val:02x},frame\n"
                )
        if stream_csv_file is not None:
            stream_csv_file.flush()
        if packets_hex_file is not None:
            packet_count += 1
            header = (
                f"--- packet {packet_count} | offset={base_index} | {ts} | "
                f"SCID={frame_scid} VC={vc_count} ---"
            )
            _write_frame_hex_block(packets_hex_file, header, raw_frame)

    def _save_dropped_window(candidate: bytes, base_index: int, state: str) -> None:
        nonlocal dropped_count, discarded_bytes
        ts = _format_timestamp()
        if dropped_hex_file is not None:
            dropped_count += 1
            header = (
                f"--- dropped {dropped_count} | offset={base_index} | {ts} | "
                f"{state} ---"
            )
            _write_frame_hex_block(dropped_hex_file, header, candidate)
        print(
            f"[TM] dropped | {ts} | offset={base_index} | {state} | CRC failed",
            flush=True,
        )
        _log_classified_byte(base_index, candidate[0], state)
        if stream_csv_file is not None:
            stream_csv_file.flush()
        discarded_bytes += 1

    def _maybe_print_stats() -> None:
        nonlocal stats_time, discarded_bytes, vc_frame_gaps
        now = time.monotonic()
        if now - stats_time >= 30.0:
            elapsed = now - stats_time
            total = sum(frames_sent_by_scid.values())
            rate = total / elapsed if elapsed else 0
            scid_counts = " | ".join(
                f"SCID {s}: {n} frames" for s, n in sorted(frames_sent_by_scid.items())
            )
            print(
                f"[TM] stats: {scid_counts or 'no frames'} | {rate:.1f} f/s | "
                f"{vc_frame_gaps} gap(s) | {discarded_bytes} discarded bytes"
            )
            stats_time = now
            frames_sent_by_scid.clear()
            vc_frame_gaps = 0
            discarded_bytes = 0

    while True:
        chunk = ser.read(max(1, ser.in_waiting or 1))
        if not chunk:
            _maybe_print_stats()
            continue
        _append_stream_hex(chunk)
        buf.extend(chunk)
        session_byte_index += len(chunk)

        while len(buf) >= frame_length:
            candidate = bytes(buf[:frame_length])
            base_index = _buf_base_index()
            if _crc16_ccitt(candidate[:-2]) == int.from_bytes(candidate[-2:], "big"):
                raw_frame = candidate
                frame_scid = ((raw_frame[0] << 8) | raw_frame[1]) >> 4 & 0x3FF

                if frame_scid != primary_scid:
                    if not locked:
                        print(
                            f"[TM] acquired lock: SCID={frame_scid} → normalizing to {primary_scid}"
                        )
                    candidate = _normalize_tm_scid(candidate, primary_scid, vc_id)

                locked = True
                _log_classified_frame(raw_frame, base_index)
                del buf[:frame_length]

                vc_count = candidate[3]
                if frame_scid in last_vc_count:
                    expected_vc = (last_vc_count[frame_scid] + 1) & 0xFF
                    if vc_count != expected_vc:
                        gap = (vc_count - last_vc_count[frame_scid]) & 0xFF
                        if gap > 1:
                            vc_frame_gaps += gap - 1
                            print(
                                f"[TM] VC frame gap (SCID {frame_scid}): expected {expected_vc}, "
                                f"got {vc_count} ({gap - 1} frame(s) lost)"
                            )
                last_vc_count[frame_scid] = vc_count

                tm_sock.sendto(candidate, (yamcs_host, tm_port))
                frames_sent_by_scid[frame_scid] += 1
            else:
                was_locked = locked
                if locked:
                    print("[TM] lost frame sync, hunting...")
                    locked = False
                state = "locked_discard" if was_locked else "hunt"
                _save_dropped_window(candidate, base_index, state)
                del buf[:1]

        _maybe_print_stats()


def _forward_tm_tcp(
    tcp_sock, tm_sock, yamcs_host: str, tm_port: int, frame_length: int
):
    """Read fixed-length TM frames from a TCP connection and forward to YAMCS via UDP.

    TCP mode uses fixed-length reads (no sync scan), so multi-SCID accept-list
    is not applied here — only the primary SCID (spacecraft_ids[0]) is relevant
    for the TC path.
    """
    print(f"[TM] TCP → UDP {yamcs_host}:{tm_port}  (frame_length={frame_length})")
    buf = b""
    while True:
        chunk = tcp_sock.recv(4096)
        if not chunk:
            print("[TM] TCP connection closed.")
            break
        buf += chunk
        while len(buf) >= frame_length:
            frame, buf = buf[:frame_length], buf[frame_length:]
            tm_sock.sendto(frame, (yamcs_host, tm_port))


# ---------------------------------------------------------------------------
# TC path helpers
# ---------------------------------------------------------------------------

# CCSDS TC Transfer Frame structure: 5-byte primary header + data + 2-byte FECF (CRC)
_TC_FRAME_HEADER_SIZE = 5
_TC_FRAME_CRC_SIZE = 2

# CCSDS Space Packet primary header is 6 bytes.
_SP_HEADER_SIZE = 6

# Thread-safe counter for CCSDS Source Sequence Count (per-APID not needed — all
# F Prime commands use APID 0).
import itertools  # noqa: E402  (stdlib, grouped with runtime helpers)

_ccsds_seq_counter = itertools.count()


def _fix_ccsds_primary_header(space_packet: bytearray) -> None:
    """Patch CCSDS Packet Data Length and Source Sequence Count in-place.

    The XTCE generated by fprime-to-xtce uses FixedValueEntry for both fields,
    encoding them as literal 0.  No built-in YAMCS command postprocessor fills
    them in without also adding a CFS secondary-header checksum that would
    corrupt the F Prime FW_PACKET_COMMAND type field.  We fix them here instead.

    CCSDS primary header layout (6 bytes / 48 bits):
      bits 47-45  Version (3)
      bit  44     Type (1)
      bit  43     Sec Hdr Flag (1)
      bits 42-32  APID (11)
      bits 31-30  Sequence Flags (2)
      bits 29-16  Source Sequence Count (14)
      bits 15-0   Packet Data Length (16)

    Packet Data Length = (total_packet_length - primary_header(6) - 1)
    """
    if len(space_packet) < _SP_HEADER_SIZE:
        return

    # --- Packet Data Length (bytes 4-5) ---
    pkt_data_len = len(space_packet) - _SP_HEADER_SIZE - 1
    space_packet[4] = (pkt_data_len >> 8) & 0xFF
    space_packet[5] = pkt_data_len & 0xFF

    # --- Source Sequence Count (lower 14 bits of bytes 2-3) ---
    seq = next(_ccsds_seq_counter) & 0x3FFF
    seq_flags = space_packet[2] & 0xC0  # preserve upper 2 bits (Sequence Flags)
    space_packet[2] = seq_flags | ((seq >> 8) & 0x3F)
    space_packet[3] = seq & 0xFF


def _extract_space_packet(tc_transfer_frame: bytes) -> bytearray:
    """Strip the TC Transfer Frame primary header and CRC, returning just the Space Packet.

    YAMCS UdpTcFrameLink sends fully-formed TC Transfer Frames.  The FSW expects
    the TC Transfer Frame payload to be auth_header + SpacePacket + HMAC — so we
    must extract the SpacePacket, apply auth, then re-wrap in a new TC frame.

    Returns a mutable bytearray so _fix_ccsds_primary_header can patch it in-place.
    """
    min_len = _TC_FRAME_HEADER_SIZE + _TC_FRAME_CRC_SIZE + 1
    if len(tc_transfer_frame) < min_len:
        raise ValueError(f"TC frame too short ({len(tc_transfer_frame)} bytes)")
    return bytearray(tc_transfer_frame[_TC_FRAME_HEADER_SIZE:-_TC_FRAME_CRC_SIZE])


def _wrap_tc(
    tc_transfer_frame: bytes,
    auth_framer: AuthenticateFramer,
    sdlink_framer: SpaceDataLinkFramerDeframer,
) -> bytes:
    """Extract SpacePacket from YAMCS TC frame, fix CCSDS header, auth-wrap, re-frame.

    FSW uplink pipeline:
      UART → CcsdsTcFrameDetector/frameAccumulator → tcDeframer → authenticate → spacePacketDeframer

    The tcDeframer strips the outer TC Transfer Frame and passes its payload to Authenticate.
    Authenticate expects: SPI(2) + SeqNum(4) + SpacePacket + HMAC(16).
    So the wire format must be: TC_Frame( SPI + SeqNum + SpacePacket + HMAC ).
    """
    space_packet = _extract_space_packet(tc_transfer_frame)
    _fix_ccsds_primary_header(space_packet)
    auth_wrapped = auth_framer.frame(
        bytes(space_packet)
    )  # SPI + SeqNum + SpacePacket + HMAC
    return sdlink_framer.frame(auth_wrapped)  # TC_header + auth_wrapped + CRC


def _forward_tc_serial(
    tc_sock,
    ser,
    auth_framer: AuthenticateFramer,
    sdlink_framer: SpaceDataLinkFramerDeframer,
):
    """Receive TC datagrams from YAMCS, extract SpacePacket, auth-wrap, re-frame, write to serial."""
    print("[TC] UDP → extract SpacePacket → authenticate → TC frame → serial")
    tc_count = 0
    while True:
        tc_transfer_frame, _ = tc_sock.recvfrom(4096)
        try:
            out_frame = _wrap_tc(tc_transfer_frame, auth_framer, sdlink_framer)
        except ValueError as exc:
            print(
                f"[TC] Malformed TC frame from YAMCS ({len(tc_transfer_frame)} bytes): {exc}"
            )
            continue
        ser.write(out_frame)
        tc_count += 1
        print(
            f"[TC] #{tc_count}: YAMCS {len(tc_transfer_frame)}B → serial {len(out_frame)}B"
        )


def _forward_tc_tcp(
    tc_sock,
    tcp_sock,
    auth_framer: AuthenticateFramer,
    sdlink_framer: SpaceDataLinkFramerDeframer,
):
    """Receive TC datagrams from YAMCS, extract SpacePacket, auth-wrap, re-frame, send over TCP."""
    print("[TC] UDP → extract SpacePacket → authenticate → TC frame → TCP")
    while True:
        tc_transfer_frame, _ = tc_sock.recvfrom(4096)
        try:
            out_frame = _wrap_tc(tc_transfer_frame, auth_framer, sdlink_framer)
        except ValueError as exc:
            print(
                f"[TC] Malformed TC frame from YAMCS ({len(tc_transfer_frame)} bytes): {exc}"
            )
            continue
        tcp_sock.sendall(out_frame)


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------


def parse_args():
    def _parse_scid_list(val: str) -> list[int]:
        scids = [int(x) for x in val.split(",") if x.strip()]
        if not scids:
            raise argparse.ArgumentTypeError(
                "--spacecraft-id must list at least one SCID"
            )
        for s in scids:
            if not 0 <= s <= 0x3FF:
                raise argparse.ArgumentTypeError(
                    f"--spacecraft-id {s} out of range (CCSDS SCID is 10 bits, 0..1023)"
                )
        return scids

    p = argparse.ArgumentParser(
        description="PROVES YAMCS authentication adapter (Option B)",
        formatter_class=argparse.ArgumentDefaultsHelpFormatter,
    )
    p.add_argument(
        "--mode",
        choices=["serial", "tcp"],
        default="serial",
        help="Transport mode: 'serial' for USB-UART (Use Case 1), 'tcp' for bent-pipe (Use Case 2)",
    )

    # Serial options
    p.add_argument("--uart-device", default="/dev/ttyUSB0", help="Serial device path")
    p.add_argument("--uart-baud", type=int, default=115200, help="Serial baud rate")

    # TCP options (Use Case 2)
    p.add_argument(
        "--tcp-host", default="127.0.0.1", help="Ground station host (tcp mode)"
    )
    p.add_argument(
        "--tcp-port", type=int, default=5000, help="Ground station port (tcp mode)"
    )

    # YAMCS UDP endpoints
    p.add_argument("--yamcs-host", default="127.0.0.1", help="YAMCS host")
    p.add_argument(
        "--yamcs-tm-port",
        type=int,
        default=50000,
        help="YAMCS TM UDP port (adapter sends TM here)",
    )
    p.add_argument(
        "--yamcs-tc-port",
        type=int,
        default=50001,
        help="YAMCS TC UDP port (adapter receives TC from here)",
    )

    # Auth options
    p.add_argument(
        "--auth-key",
        default=None,
        help="HMAC key as hex string (no 0x prefix). Defaults to key from AuthDefaultKey.h.",
    )

    # Frame size and CCSDS identifiers
    p.add_argument(
        "--frame-length",
        type=int,
        default=248,
        help="TM frame length in bytes (must match TmFrameFixedSize / YAMCS frameLength)",
    )
    p.add_argument(
        "--spacecraft-id",
        dest="spacecraft_ids",
        type=_parse_scid_list,
        action="append",
        default=None,
        help=(
            "Primary spacecraft ID used for TC framing and TM SCID normalization. "
            "TM accepts frames from any SCID and normalizes them to this value "
            "before forwarding to YAMCS. Comma-separated or repeatable; first "
            "entry is the primary. Default: 68."
        ),
    )
    p.add_argument(
        "--vc-id",
        type=int,
        default=1,
        help="CCSDS virtual channel ID (3-bit, used for frame sync header)",
    )

    args = p.parse_args()
    # Flatten [[68, 67], [65]] → [68, 67, 65]; default to [68].
    if args.spacecraft_ids is None:
        args.spacecraft_ids = [68]
    else:
        args.spacecraft_ids = [s for group in args.spacecraft_ids for s in group]
    return args


def main():
    args = parse_args()

    # Resolve auth key
    auth_key = args.auth_key
    if auth_key is None:
        auth_key = get_default_auth_key_from_header()
        print("[auth] Loaded key from AuthDefaultKey.h")

    auth_framer = AuthenticateFramer(authentication_key=auth_key)

    # SpaceDataLinkFramerDeframer builds the TC Transfer Frame that wraps
    # auth(SpacePacket) for the FSW uplink pipeline (tcDeframer → authenticate).
    sdlink_framer = SpaceDataLinkFramerDeframer(
        scid=args.spacecraft_ids[0],
        vcid=args.vc_id,
        frame_size=args.frame_length,
    )

    # Shared UDP sockets
    tm_sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    tc_sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    tc_sock.bind(("0.0.0.0", args.yamcs_tc_port))

    session_dir: Path | None = None

    if args.mode == "serial":
        import serial  # pyserial — installed via requirements.txt

        start_stamp = datetime.now().strftime("%Y-%m-%d_%H-%M-%S")
        session_dir = data_path / start_stamp
        session_dir.mkdir(parents=True, exist_ok=True)
        stream_csv = session_dir / "stream.csv"
        stream_hex = session_dir / "stream.hex"
        packets_hex = session_dir / "packets.hex"
        dropped_hex = session_dir / "dropped.hex"
        with stream_csv.open("w") as f:
            f.write("timestamp,byte_index,hex_value,state\n")
        print(
            f"[adapter] TM capture → {session_dir}/ "
            f"(stream.csv, stream.hex, packets.hex, dropped.hex)"
        )

        print(f"[serial] Opening {args.uart_device} @ {args.uart_baud} baud")
        ser = serial.Serial(args.uart_device, args.uart_baud, timeout=0.1)

        # Enlarge the OS receive buffer so burst TM frames from the FSW don't
        # overflow while the adapter is in the sendto() or CRC-check path.
        # The default on macOS is only 4 KB (~16 frames at 248 B each).
        try:
            ser.set_buffer_size(rx_size=65536)
        except Exception:
            pass  # not supported on all platforms; 4 KB default still works

        tm_thread = threading.Thread(
            target=_forward_tm_serial,
            args=(
                ser,
                tm_sock,
                args.yamcs_host,
                args.yamcs_tm_port,
                args.frame_length,
                args.spacecraft_ids,
                args.vc_id,
                stream_csv,
                stream_hex,
                packets_hex,
                dropped_hex,
            ),
            daemon=True,
        )
        tc_thread = threading.Thread(
            target=_forward_tc_serial,
            args=(tc_sock, ser, auth_framer, sdlink_framer),
            daemon=True,
        )

    elif args.mode == "tcp":
        print(f"[tcp] Connecting to ground station at {args.tcp_host}:{args.tcp_port}")
        tcp_sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        tcp_sock.connect((args.tcp_host, args.tcp_port))
        print("[tcp] Connected.")

        tm_thread = threading.Thread(
            target=_forward_tm_tcp,
            args=(
                tcp_sock,
                tm_sock,
                args.yamcs_host,
                args.yamcs_tm_port,
                args.frame_length,
            ),
            daemon=True,
        )
        tc_thread = threading.Thread(
            target=_forward_tc_tcp,
            args=(tc_sock, tcp_sock, auth_framer, sdlink_framer),
            daemon=True,
        )

    print(
        f"[adapter] Starting in '{args.mode}' mode. YAMCS at {args.yamcs_host} (TM→:{args.yamcs_tm_port}, TC←:{args.yamcs_tc_port})"
    )
    tm_thread.start()
    tc_thread.start()

    try:
        tm_thread.join()
        tc_thread.join()
    except KeyboardInterrupt:
        print("\n[adapter] Interrupted. Shutting down.")
        if session_dir is not None:
            print(f"[adapter] TM capture saved to {session_dir}/")
        sys.exit(0)


if __name__ == "__main__":
    main()

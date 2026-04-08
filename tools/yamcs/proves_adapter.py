"""
PROVES YAMCS Adapter — Option B authentication bridge.

Routes TM frames from the spacecraft to YAMCS (via UDP) and TC frames from
YAMCS (via UDP) back to the spacecraft after wrapping them with the HMAC-SHA256
authentication header/trailer required by the FSW Authenticate component.

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
from pathlib import Path

# Flush stdout on every print so diagnostic messages appear immediately even
# when the adapter is run as a background process or piped to a log file.
sys.stdout.reconfigure(line_buffering=True)

# Allow importing authenticate_plugin from the Framing package.  Must be set
# before the import below so authenticate_plugin.py (and fprime_gds) load OK.
sys.path.insert(0, str(Path(__file__).parents[2] / "Framing" / "src"))

# ---------------------------------------------------------------------------
# CRC16-CCITT (CCSDS standard: poly 0x1021, init 0xFFFF, no reflection)
# ---------------------------------------------------------------------------


def _crc16_ccitt(data: bytes) -> int:
    crc = 0xFFFF
    for byte in data:
        crc ^= byte << 8
        for _ in range(8):
            crc = ((crc << 1) ^ 0x1021 if crc & 0x8000 else crc << 1) & 0xFFFF
    return crc


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


def _serial_read_exact(ser, n: int) -> bytes:
    """Read exactly n bytes from serial, blocking until all bytes are available."""
    buf = b""
    while len(buf) < n:
        chunk = ser.read(n - len(buf))
        if chunk:
            buf += chunk
    return buf


def _sync_serial_frame(ser, frame_length: int, sync_header: bytes) -> bytes:
    """Scan the serial stream for a valid frame boundary and return the first complete frame.

    The OS serial buffer may contain a partial frame from before the adapter
    started (or the header bytes may appear in payload data as a false positive).
    Scan byte by byte: each time the header pattern is found, read a full candidate
    frame and validate its CRC. Only accept when the CRC is correct.
    """
    print(f"[TM] Searching for frame sync (header={sync_header.hex(' ')})...")
    header_len = len(sync_header)
    window = b""
    attempts = 0
    while True:
        b = ser.read(1)
        if not b:
            continue
        window = (window + b)[-header_len:]
        if window != sync_header:
            continue
        # Candidate sync point — read the rest of the frame and validate CRC.
        remainder = _serial_read_exact(ser, frame_length - header_len)
        frame = sync_header + remainder
        if _crc16_ccitt(frame[:-2]) == int.from_bytes(frame[-2:], "big"):
            print(f"[TM] Frame sync acquired (after {attempts} false positives).")
            return frame
        # CRC mismatch — false positive, keep scanning from the next byte.
        attempts += 1
        window = b""  # reset window so we re-scan from inside the false frame


def _forward_tm_serial(
    ser,
    tm_sock,
    yamcs_host: str,
    tm_port: int,
    frame_length: int,
    spacecraft_id: int = 68,
    vc_id: int = 1,
):
    """Read fixed-length TM frames from serial and forward to YAMCS via UDP."""
    print(f"[TM] serial → UDP {yamcs_host}:{tm_port}  (frame_length={frame_length})")
    # Compute the expected first 2 bytes of the CCSDS TM primary header:
    # bits 15-14: version=0, bits 13-4: spacecraft_id, bits 3-1: vc_id, bit 0: OCF=0
    word0 = (spacecraft_id << 4) | (vc_id << 1)
    sync_header = bytes([(word0 >> 8) & 0xFF, word0 & 0xFF])

    # Scan to the first frame boundary before entering the steady-state read loop.
    frame = _sync_serial_frame(ser, frame_length, sync_header)
    tm_sock.sendto(frame, (yamcs_host, tm_port))
    frames_sent = 1
    crc_errors = 0

    while True:
        # Accumulate bytes until a complete frame is assembled. pyserial's
        # read(n) with a short timeout may return fewer than n bytes.
        frame = _serial_read_exact(ser, frame_length)

        # Validate CRC in steady state to detect alignment drift (serial glitches,
        # board reset, USB hiccup, etc.).  Re-sync byte-by-byte on failure rather
        # than forwarding bad data to YAMCS and staying permanently misaligned.
        if _crc16_ccitt(frame[:-2]) != int.from_bytes(frame[-2:], "big"):
            crc_errors += 1
            print(
                f"[TM] CRC error in steady state (#{crc_errors} after {frames_sent} "
                f"good frames) — first 6 bytes: {frame[:6].hex(' ')} — resyncing..."
            )
            frame = _sync_serial_frame(ser, frame_length, sync_header)
            frames_sent = 0
            crc_errors = 0

        tm_sock.sendto(frame, (yamcs_host, tm_port))
        frames_sent += 1


def _forward_tm_tcp(
    tcp_sock, tm_sock, yamcs_host: str, tm_port: int, frame_length: int
):
    """Read fixed-length TM frames from a TCP connection and forward to YAMCS via UDP."""
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
        type=int,
        default=68,
        help="CCSDS spacecraft ID (10-bit, used for frame sync header)",
    )
    p.add_argument(
        "--vc-id",
        type=int,
        default=1,
        help="CCSDS virtual channel ID (3-bit, used for frame sync header)",
    )

    return p.parse_args()


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
        scid=args.spacecraft_id,
        vcid=args.vc_id,
        frame_size=args.frame_length,
    )

    # Shared UDP sockets
    tm_sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    tc_sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    tc_sock.bind(("0.0.0.0", args.yamcs_tc_port))

    if args.mode == "serial":
        import serial  # pyserial — installed via requirements.txt

        print(f"[serial] Opening {args.uart_device} @ {args.uart_baud} baud")
        ser = serial.Serial(args.uart_device, args.uart_baud, timeout=0.1)

        tm_thread = threading.Thread(
            target=_forward_tm_serial,
            args=(
                ser,
                tm_sock,
                args.yamcs_host,
                args.yamcs_tm_port,
                args.frame_length,
                args.spacecraft_id,
                args.vc_id,
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
        sys.exit(0)


if __name__ == "__main__":
    main()

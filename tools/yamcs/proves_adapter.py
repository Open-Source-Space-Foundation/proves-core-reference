"""
PROVES YAMCS Adapter — Option B authentication bridge.

Routes TM frames from the spacecraft to YAMCS (via UDP) and TC frames from
YAMCS (via UDP) back to the spacecraft after wrapping them with the HMAC-SHA256
authentication header/trailer required by the FSW Authenticate component.

The TM side accepts valid CCSDS frames from any configured spacecraft ID and
routes each frame to the matching YAMCS deployment port without rewriting the
frame.  TC from each deployment is received on its configured UDP port and
wrapped with that deployment's spacecraft ID before being written to the radio
link.

Use Case 1 (local UART):
    python proves_adapter.py --mode serial --uart-device /dev/ttyUSB0

Use Case 2 (remote TCP, skeleton):
    python proves_adapter.py --mode tcp --tcp-host <gs-host> --tcp-port 5000 \
        --yamcs-host <server-ip>
"""

import argparse
import json
import socket
import sys
import threading
from collections import Counter
from dataclasses import dataclass
from datetime import datetime, timezone
from pathlib import Path

import yaml

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
    """Compute CRC16-CCITT over the given bytes."""
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
# YAMCS deployment configuration
# ---------------------------------------------------------------------------


@dataclass(frozen=True)
class YamcsDeployment:
    """One YAMCS instance the adapter routes TM to and receives TC from."""

    name: str
    spacecraft_id: int
    vc_id: int
    yamcs_host: str
    tm_port: int
    tc_port: int


def _validate_u10(value: int, field_name: str) -> None:
    """Require a 10-bit unsigned value (CCSDS spacecraft ID range)."""
    if not 0 <= value <= 0x3FF:
        raise ValueError(f"{field_name} {value} out of range (expected 0..1023)")


def _validate_vc_id(value: int, field_name: str) -> None:
    """Require a 3-bit virtual channel ID."""
    if not 0 <= value <= 0x7:
        raise ValueError(f"{field_name} {value} out of range (expected 0..7)")


def _deployment_from_mapping(
    item: dict,
    *,
    default_host: str = "127.0.0.1",
    default_vc_id: int = 1,
) -> YamcsDeployment:
    """Build a validated YamcsDeployment from one deployments.yaml entry."""
    if not isinstance(item, dict):
        raise ValueError("each deployment must be a mapping")

    required = ("name", "spacecraft_id", "tm_port", "tc_port")
    missing = [field for field in required if field not in item]
    if missing:
        raise ValueError(f"deployment missing required field(s): {', '.join(missing)}")

    deployment = YamcsDeployment(
        name=str(item["name"]),
        spacecraft_id=int(item["spacecraft_id"]),
        vc_id=int(item.get("vc_id", default_vc_id)),
        yamcs_host=str(item.get("yamcs_host", default_host)),
        tm_port=int(item["tm_port"]),
        tc_port=int(item["tc_port"]),
    )
    _validate_u10(deployment.spacecraft_id, f"{deployment.name}.spacecraft_id")
    _validate_vc_id(deployment.vc_id, f"{deployment.name}.vc_id")
    for field_name in ("tm_port", "tc_port"):
        port = getattr(deployment, field_name)
        if not 1 <= port <= 65535:
            raise ValueError(f"{deployment.name}.{field_name} {port} out of range")
    if not deployment.name.strip():
        raise ValueError("deployment name must not be empty")
    return deployment


def _validate_deployments(deployments: list[YamcsDeployment]) -> list[YamcsDeployment]:
    """Reject empty deployment lists and duplicate names/SCIDs/TC ports."""
    if not deployments:
        raise ValueError("at least one YAMCS deployment is required")

    seen_names: set[str] = set()
    seen_scids: set[int] = set()
    seen_tc_ports: set[int] = set()
    for deployment in deployments:
        if deployment.name in seen_names:
            raise ValueError(f"duplicate YAMCS deployment name: {deployment.name}")
        if deployment.spacecraft_id in seen_scids:
            raise ValueError(f"duplicate spacecraft_id: {deployment.spacecraft_id}")
        if deployment.tc_port in seen_tc_ports:
            raise ValueError(f"duplicate tc_port: {deployment.tc_port}")
        seen_names.add(deployment.name)
        seen_scids.add(deployment.spacecraft_id)
        seen_tc_ports.add(deployment.tc_port)
    return deployments


def load_yamcs_deployments(path: Path) -> list[YamcsDeployment]:
    """Load and validate the deployments list from a YAML routing table."""
    with path.open() as stream:
        config = yaml.safe_load(stream)
    if not isinstance(config, dict) or not isinstance(config.get("deployments"), list):
        raise ValueError("deployment config must contain a 'deployments' list")
    deployments = [_deployment_from_mapping(item) for item in config["deployments"]]
    return _validate_deployments(deployments)


def _default_deployments(args) -> list[YamcsDeployment]:
    """Build the single-deployment fallback from the CLI arguments."""
    primary_scid = args.spacecraft_ids[0]
    return _validate_deployments(
        [
            YamcsDeployment(
                name=f"scid-{primary_scid}",
                spacecraft_id=primary_scid,
                vc_id=args.vc_id,
                yamcs_host=args.yamcs_host,
                tm_port=args.yamcs_tm_port,
                tc_port=args.yamcs_tc_port,
            )
        ]
    )


def resolve_yamcs_deployments(args) -> list[YamcsDeployment]:
    """Return deployments from --yamcs-deployments or the CLI fallback."""
    if args.yamcs_deployments is not None:
        return load_yamcs_deployments(args.yamcs_deployments)
    return _default_deployments(args)


# ---------------------------------------------------------------------------
# TM path helpers
# ---------------------------------------------------------------------------


def _extract_tm_scid(frame: bytes) -> int:
    """Extract the 10-bit spacecraft ID from a TM frame header."""
    return ((frame[0] << 8) | frame[1]) >> 4 & 0x3FF


def _tm_route_for_frame(
    frame: bytes, deployments_by_scid: dict[int, YamcsDeployment]
) -> YamcsDeployment | None:
    """Return the deployment whose SCID matches the frame, if configured."""
    return deployments_by_scid.get(_extract_tm_scid(frame))


# Default drop log for CRC-valid TM frames whose SCID matches no deployment.
DEFAULT_UNKNOWN_PKTS_LOG = Path("unknown_pkts.json")


def _log_unknown_frame(log_path: Path, frame: bytes) -> None:
    """Append a CRC-valid frame with an unconfigured SCID to the drop log.

    One JSON object per line (JSON Lines) so the file can be tailed, grepped,
    or parsed incrementally while the adapter keeps running.  Logging failures
    must never take down the TM path, so errors are reported and swallowed.
    """
    record = {
        "time": datetime.now(timezone.utc).isoformat(timespec="milliseconds"),
        "scid": _extract_tm_scid(frame),
        "vc_id": (frame[1] >> 1) & 0x7,
        "vc_count": frame[3],
        "length": len(frame),
        "frame_hex": frame.hex(),
    }
    try:
        with log_path.open("a") as stream:
            stream.write(json.dumps(record) + "\n")
    except OSError as exc:
        print(f"[TM] failed to write {log_path}: {exc}")


def _forward_tm_serial(
    ser,
    tm_sock,
    deployments: list[YamcsDeployment],
    frame_length: int,
    unknown_log: Path = DEFAULT_UNKNOWN_PKTS_LOG,
):
    """Read fixed-length TM frames from serial and forward to YAMCS via UDP.

    Accepts frames from *any* spacecraft ID using a CRC-16/CCITT-only HUNT/LOCK
    state machine.  Valid frames are forwarded unchanged to the deployment whose
    configured SCID matches the frame header.  Valid frames with unknown SCIDs
    are counted and dropped so operators can see unconfigured satellites.

      HUNT — slide 1 byte at a time until a CRC-valid frame is found.
      LOCK — once in sync, take the next frame exactly ``frame_length`` bytes
             later; fall back to HUNT on CRC failure.
    """
    import time

    deployments_by_scid = {item.spacecraft_id: item for item in deployments}
    print(
        f"[TM] serial → routed UDP ({len(deployments)} deployment(s), frame_length={frame_length})"
    )
    for deployment in deployments:
        print(
            f"[TM] SCID {deployment.spacecraft_id} → "
            f"{deployment.yamcs_host}:{deployment.tm_port} ({deployment.name})"
        )

    buf = bytearray()
    frames_sent_by_scid: Counter[int] = Counter()
    frames_dropped_by_scid: Counter[int] = Counter()
    last_vc_count: dict[int, int] = {}
    vc_frame_gaps = 0
    junk_bytes = 0
    locked = False
    stats_time = time.monotonic()

    def _maybe_print_stats() -> None:
        """Print and reset routing statistics every 30 seconds."""
        nonlocal stats_time, junk_bytes, vc_frame_gaps
        now = time.monotonic()
        if now - stats_time >= 30.0:
            elapsed = now - stats_time
            total = sum(frames_sent_by_scid.values())
            rate = total / elapsed if elapsed else 0
            scid_counts = " | ".join(
                f"SCID {s}: {n} frames" for s, n in sorted(frames_sent_by_scid.items())
            )
            dropped_counts = " | ".join(
                f"SCID {s}: {n} dropped"
                for s, n in sorted(frames_dropped_by_scid.items())
            )
            print(
                f"[TM] stats: {scid_counts or 'no frames'} | {rate:.1f} f/s | "
                f"{vc_frame_gaps} gap(s) | {junk_bytes} junk bytes | "
                f"{dropped_counts or 'no dropped SCIDs'}"
            )
            stats_time = now
            frames_sent_by_scid.clear()
            frames_dropped_by_scid.clear()
            vc_frame_gaps = 0
            junk_bytes = 0

    while True:
        chunk = ser.read(max(1, ser.in_waiting or 1))
        if not chunk:
            _maybe_print_stats()
            continue
        buf.extend(chunk)

        while len(buf) >= frame_length:
            candidate = bytes(buf[:frame_length])
            if _crc16_ccitt(candidate[:-2]) == int.from_bytes(candidate[-2:], "big"):
                frame_scid = _extract_tm_scid(candidate)
                locked = True
                del buf[:frame_length]

                deployment = _tm_route_for_frame(candidate, deployments_by_scid)
                if deployment is None:
                    frames_dropped_by_scid[frame_scid] += 1
                    _log_unknown_frame(unknown_log, candidate)
                    if frames_dropped_by_scid[frame_scid] == 1:
                        print(
                            f"[TM] dropping unconfigured SCID {frame_scid} "
                            f"(logging to {unknown_log})"
                        )
                    continue

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

                tm_sock.sendto(candidate, (deployment.yamcs_host, deployment.tm_port))
                frames_sent_by_scid[frame_scid] += 1
            else:
                if locked:
                    print("[TM] lost frame sync, hunting...")
                    locked = False
                junk_bytes += 1
                del buf[:1]

        _maybe_print_stats()


def _forward_tm_tcp(
    tcp_sock,
    tm_sock,
    deployments: list[YamcsDeployment],
    frame_length: int,
    unknown_log: Path = DEFAULT_UNKNOWN_PKTS_LOG,
):
    """Read fixed-length TM frames from a TCP connection and forward to YAMCS via UDP.

    TCP mode uses fixed-length reads (no sync scan), but valid frames are still
    routed by SCID to the matching deployment.
    """
    deployments_by_scid = {item.spacecraft_id: item for item in deployments}
    print(
        f"[TM] TCP → routed UDP ({len(deployments)} deployment(s), "
        f"frame_length={frame_length})"
    )
    buf = b""
    while True:
        chunk = tcp_sock.recv(4096)
        if not chunk:
            print("[TM] TCP connection closed.")
            break
        buf += chunk
        while len(buf) >= frame_length:
            frame, buf = buf[:frame_length], buf[frame_length:]
            deployment = _tm_route_for_frame(frame, deployments_by_scid)
            if deployment is None:
                _log_unknown_frame(unknown_log, frame)
                print(f"[TM] dropping unconfigured SCID {_extract_tm_scid(frame)}")
                continue
            tm_sock.sendto(frame, (deployment.yamcs_host, deployment.tm_port))


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
    deployment: YamcsDeployment,
    write_lock: threading.Lock,
):
    """Receive TC datagrams from YAMCS, extract SpacePacket, auth-wrap, re-frame, write to serial."""
    print(
        f"[TC] {deployment.name} UDP:{deployment.tc_port} → authenticate → "
        f"SCID {deployment.spacecraft_id} TC frame → serial"
    )
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
        with write_lock:
            ser.write(out_frame)
        tc_count += 1
        print(
            f"[TC] {deployment.name} #{tc_count}: YAMCS {len(tc_transfer_frame)}B "
            f"→ serial {len(out_frame)}B"
        )


def _forward_tc_tcp(
    tc_sock,
    tcp_sock,
    auth_framer: AuthenticateFramer,
    sdlink_framer: SpaceDataLinkFramerDeframer,
    deployment: YamcsDeployment,
    write_lock: threading.Lock,
):
    """Receive TC datagrams from YAMCS, extract SpacePacket, auth-wrap, re-frame, send over TCP."""
    print(
        f"[TC] {deployment.name} UDP:{deployment.tc_port} → authenticate → "
        f"SCID {deployment.spacecraft_id} TC frame → TCP"
    )
    while True:
        tc_transfer_frame, _ = tc_sock.recvfrom(4096)
        try:
            out_frame = _wrap_tc(tc_transfer_frame, auth_framer, sdlink_framer)
        except ValueError as exc:
            print(
                f"[TC] Malformed TC frame from YAMCS ({len(tc_transfer_frame)} bytes): {exc}"
            )
            continue
        with write_lock:
            tcp_sock.sendall(out_frame)


def _bind_tc_socket(deployment: YamcsDeployment):
    """Bind a UDP socket on the deployment's TC port."""
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.bind(("0.0.0.0", deployment.tc_port))
    return sock


def _sdlink_framer_for_deployment(
    deployment: YamcsDeployment, frame_length: int
) -> SpaceDataLinkFramerDeframer:
    """Build a TC framer stamped with the deployment's SCID/VCID."""
    return SpaceDataLinkFramerDeframer(
        scid=deployment.spacecraft_id,
        vcid=deployment.vc_id,
        frame_size=frame_length,
    )


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------


def parse_args():
    """Parse command-line arguments."""

    def _parse_scid_list(val: str) -> list[int]:
        """Parse a comma-separated list of 10-bit spacecraft IDs."""
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
        "--yamcs-deployments",
        type=Path,
        default=None,
        help="YAML file listing one or more YAMCS deployments to route by SCID",
    )
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

    p.add_argument(
        "--unknown-pkts-log",
        type=Path,
        default=DEFAULT_UNKNOWN_PKTS_LOG,
        help="JSON-lines file where CRC-valid TM frames with unconfigured SCIDs are appended",
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
            "Spacecraft ID for the single-deployment fallback. Comma-separated "
            "or repeatable values are accepted for backward compatibility, but "
            "the first entry is used unless --yamcs-deployments is provided. "
            "Default: 68."
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
    """Run the adapter: route TM to YAMCS and auth-wrap TC back to the radio."""
    args = parse_args()
    try:
        deployments = resolve_yamcs_deployments(args)
    except ValueError as exc:
        print(
            f"[adapter] Invalid YAMCS deployment configuration: {exc}",
            file=sys.stderr,
        )
        return 2

    print("[adapter] YAMCS deployments:")
    for deployment in deployments:
        print(
            f"  - {deployment.name}: SCID={deployment.spacecraft_id} "
            f"VCID={deployment.vc_id} TM→{deployment.yamcs_host}:{deployment.tm_port} "
            f"TC←:{deployment.tc_port}"
        )

    # Resolve auth key
    auth_key = args.auth_key
    if auth_key is None:
        auth_key = get_default_auth_key_from_header()
        print("[auth] Loaded key from AuthDefaultKey.h")

    auth_framer = AuthenticateFramer(authentication_key=auth_key)

    # Shared UDP sockets
    tm_sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    tc_sockets = {deployment: _bind_tc_socket(deployment) for deployment in deployments}
    write_lock = threading.Lock()
    threads: list[threading.Thread] = []

    if args.mode == "serial":
        import serial  # pyserial — installed via requirements.txt

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
                deployments,
                args.frame_length,
                args.unknown_pkts_log,
            ),
            daemon=True,
        )
        threads.append(tm_thread)
        for deployment, tc_sock in tc_sockets.items():
            tc_thread = threading.Thread(
                target=_forward_tc_serial,
                args=(
                    tc_sock,
                    ser,
                    auth_framer,
                    _sdlink_framer_for_deployment(deployment, args.frame_length),
                    deployment,
                    write_lock,
                ),
                daemon=True,
            )
            threads.append(tc_thread)

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
                deployments,
                args.frame_length,
                args.unknown_pkts_log,
            ),
            daemon=True,
        )
        threads.append(tm_thread)
        for deployment, tc_sock in tc_sockets.items():
            tc_thread = threading.Thread(
                target=_forward_tc_tcp,
                args=(
                    tc_sock,
                    tcp_sock,
                    auth_framer,
                    _sdlink_framer_for_deployment(deployment, args.frame_length),
                    deployment,
                    write_lock,
                ),
                daemon=True,
            )
            threads.append(tc_thread)

    print(
        f"[adapter] Starting in '{args.mode}' mode with {len(deployments)} YAMCS deployment(s)"
    )
    for thread in threads:
        thread.start()

    try:
        for thread in threads:
            thread.join()
    except KeyboardInterrupt:
        print("\n[adapter] Interrupted. Shutting down.")
        sys.exit(0)


if __name__ == "__main__":
    sys.exit(main())

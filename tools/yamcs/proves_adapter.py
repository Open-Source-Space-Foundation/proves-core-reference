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

# Allow importing authenticate_plugin from the Framing package without installing it.
sys.path.insert(0, str(Path(__file__).parents[2] / "Framing" / "src"))

from authenticate_plugin import (  # noqa: E402
    AuthenticateFramer,
    get_default_auth_key_from_header,
)

# ---------------------------------------------------------------------------
# TM path helpers
# ---------------------------------------------------------------------------


def _forward_tm_serial(ser, tm_sock, yamcs_host: str, tm_port: int, frame_length: int):
    """Read fixed-length TM frames from serial and forward to YAMCS via UDP."""
    print(f"[TM] serial → UDP {yamcs_host}:{tm_port}  (frame_length={frame_length})")
    while True:
        # Accumulate bytes until a complete frame is assembled. pyserial's
        # read(n) with a short timeout may return fewer than n bytes, so loop
        # until we have exactly frame_length bytes before forwarding.
        buf = b""
        while len(buf) < frame_length:
            chunk = ser.read(frame_length - len(buf))
            if chunk:
                buf += chunk
        tm_sock.sendto(buf, (yamcs_host, tm_port))


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


def _forward_tc_serial(tc_sock, ser, auth_framer: AuthenticateFramer):
    """Receive TC datagrams from YAMCS, wrap with auth, write to serial."""
    print("[TC] UDP → authenticate → serial")
    while True:
        tc_frame, _ = tc_sock.recvfrom(4096)
        wrapped = auth_framer.frame(tc_frame)
        ser.write(wrapped)


def _forward_tc_tcp(tc_sock, tcp_sock, auth_framer: AuthenticateFramer):
    """Receive TC datagrams from YAMCS, wrap with auth, send over TCP."""
    print("[TC] UDP → authenticate → TCP")
    while True:
        tc_frame, _ = tc_sock.recvfrom(4096)
        wrapped = auth_framer.frame(tc_frame)
        tcp_sock.sendall(wrapped)


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

    # Frame size
    p.add_argument(
        "--frame-length",
        type=int,
        default=248,
        help="TM frame length in bytes (must match TmFrameFixedSize / YAMCS frameLength)",
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
            args=(ser, tm_sock, args.yamcs_host, args.yamcs_tm_port, args.frame_length),
            daemon=True,
        )
        tc_thread = threading.Thread(
            target=_forward_tc_serial,
            args=(tc_sock, ser, auth_framer),
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
            args=(tc_sock, tcp_sock, auth_framer),
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

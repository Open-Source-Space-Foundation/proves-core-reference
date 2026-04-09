"""
PROVES Ground Station Relay — forwards TM/TC between a local serial link
and a remote YAMCS adapter over TCP.

Run this on the device physically connected to the flight software (e.g. a
Raspberry Pi).  The remote adapter (``proves_adapter.py --mode tcp``) connects
to this relay as a TCP client.

Dependencies: pyserial + frame_utils.py (no fprime_gds required).

Usage:
    python gs_relay.py --uart-device /dev/ttyUSB0 --listen-port 5000
"""

import argparse
import socket
import sys
import threading

sys.stdout.reconfigure(line_buffering=True)

from frame_utils import (  # noqa: E402
    compute_sync_header,
    read_validated_frames,
    recv_exact,
)


def _forward_tm(ser, conn, frame_length, sync_header):
    """Read CRC-validated TM frames from serial → send over TCP to the adapter."""
    print("[TM] serial → TCP relay started")

    def _serial_read(max_bytes):
        return ser.read(max(1, ser.in_waiting or 1))

    try:
        for frame, _vc_count in read_validated_frames(
            _serial_read, frame_length, sync_header
        ):
            conn.sendall(frame)
    except (BrokenPipeError, ConnectionResetError, OSError) as exc:
        print(f"[TM] TCP send error: {exc}")


def _forward_tc(conn, ser, frame_length):
    """Read TC frames from TCP → write to serial for the FSW."""
    print("[TC] TCP → serial relay started")
    try:
        while True:
            frame = recv_exact(conn.recv, frame_length)
            if not frame:
                print("[TC] TCP connection closed.")
                break
            ser.write(frame)
    except (ConnectionResetError, OSError) as exc:
        print(f"[TC] TCP recv error: {exc}")


def serve(args):
    import serial as pyserial

    print(f"[serial] Opening {args.uart_device} @ {args.uart_baud} baud")
    ser = pyserial.Serial(args.uart_device, args.uart_baud, timeout=0.1)
    try:
        ser.set_buffer_size(rx_size=65536)
    except Exception:
        pass

    sync_header = compute_sync_header(args.spacecraft_id, args.vc_id)

    srv = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    srv.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    srv.bind((args.listen_host, args.listen_port))
    srv.listen(1)
    print(f"[relay] Listening on {args.listen_host}:{args.listen_port}")

    while True:
        print("[relay] Waiting for adapter connection...")
        conn, addr = srv.accept()
        print(f"[relay] Adapter connected from {addr}")

        tm_thread = threading.Thread(
            target=_forward_tm,
            args=(ser, conn, args.frame_length, sync_header),
            daemon=True,
        )
        tc_thread = threading.Thread(
            target=_forward_tc,
            args=(conn, ser, args.frame_length),
            daemon=True,
        )

        tm_thread.start()
        tc_thread.start()

        # Wait for either thread to finish (indicates TCP disconnect).
        tm_thread.join()
        tc_thread.join()

        try:
            conn.close()
        except Exception:
            pass
        print("[relay] Adapter disconnected. Returning to accept loop.")


def parse_args():
    p = argparse.ArgumentParser(
        description="PROVES ground station relay (serial ↔ TCP)",
        formatter_class=argparse.ArgumentDefaultsHelpFormatter,
    )
    p.add_argument("--uart-device", default="/dev/ttyUSB0", help="Serial device path")
    p.add_argument("--uart-baud", type=int, default=115200, help="Serial baud rate")
    p.add_argument("--listen-host", default="0.0.0.0", help="TCP listen address")
    p.add_argument("--listen-port", type=int, default=5000, help="TCP listen port")
    p.add_argument(
        "--frame-length",
        type=int,
        default=248,
        help="TM frame length in bytes",
    )
    p.add_argument(
        "--spacecraft-id", type=int, default=68, help="CCSDS spacecraft ID (10-bit)"
    )
    p.add_argument(
        "--vc-id", type=int, default=1, help="CCSDS virtual channel ID (3-bit)"
    )
    return p.parse_args()


def main():
    args = parse_args()
    try:
        serve(args)
    except KeyboardInterrupt:
        print("\n[relay] Interrupted. Shutting down.")
        sys.exit(0)


if __name__ == "__main__":
    main()

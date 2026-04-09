"""
Shared CCSDS frame utilities for the PROVES YAMCS tooling.

This module has **zero fprime_gds dependencies** so it can run on a
resource-constrained ground station device (e.g. Raspberry Pi) that only
has pyserial installed.

Used by both ``proves_adapter.py`` and ``gs_relay.py``.
"""

import time

# ---------------------------------------------------------------------------
# CRC16-CCITT (CCSDS standard: poly 0x1021, init 0xFFFF, no reflection)
# ---------------------------------------------------------------------------


def crc16_ccitt(data: bytes) -> int:
    """Compute CRC16-CCITT over *data*."""
    crc = 0xFFFF
    for byte in data:
        crc ^= byte << 8
        for _ in range(8):
            crc = ((crc << 1) ^ 0x1021 if crc & 0x8000 else crc << 1) & 0xFFFF
    return crc


# ---------------------------------------------------------------------------
# Sync header
# ---------------------------------------------------------------------------


def compute_sync_header(spacecraft_id: int = 68, vc_id: int = 1) -> bytes:
    """Return the 2-byte CCSDS TM Transfer Frame primary header prefix.

    Bits 15-14: version=0, bits 13-4: spacecraft_id (10-bit),
    bits 3-1: vc_id (3-bit), bit 0: OCF flag=0.
    """
    word0 = (spacecraft_id << 4) | (vc_id << 1)
    return bytes([(word0 >> 8) & 0xFF, word0 & 0xFF])


# ---------------------------------------------------------------------------
# Exact-read helper
# ---------------------------------------------------------------------------


def recv_exact(read_fn, n: int) -> bytes:
    """Read exactly *n* bytes using *read_fn(max_bytes) -> bytes*.

    Returns the accumulated bytes, or an empty ``bytes`` if *read_fn*
    returns empty (EOF / disconnect) before *n* bytes are collected.
    """
    buf = b""
    while len(buf) < n:
        chunk = read_fn(n - len(buf))
        if not chunk:
            return b""
        buf += chunk
    return buf


# ---------------------------------------------------------------------------
# Rolling-buffer frame reader
# ---------------------------------------------------------------------------


def read_validated_frames(
    read_fn,
    frame_length: int,
    sync_header: bytes,
    *,
    stats_interval: float = 30.0,
    label: str = "TM",
):
    """Yield CRC-validated CCSDS TM frames from a byte stream.

    Parameters
    ----------
    read_fn:
        Callable ``read_fn(max_bytes) -> bytes``.  For serial ports this is
        ``ser.read``; for sockets wrap with a lambda that calls ``recv``.
        Must return ``b""`` on EOF / disconnect.
    frame_length:
        Expected frame size in bytes (e.g. 248).
    sync_header:
        2-byte pattern returned by :func:`compute_sync_header`.
    stats_interval:
        Seconds between periodic stats log lines (0 to disable).
    label:
        Prefix for log messages (e.g. ``"TM"``).

    Yields
    ------
    tuple of (bytes, int)
        ``(frame, vc_count)`` where *frame* is a validated frame and
        *vc_count* is the VC frame counter (byte 3 of the TM header).
    """
    buf = bytearray()
    frames_found = 0
    last_vc_count = -1
    vc_frame_gaps = 0
    junk_bytes = 0
    stats_time = time.monotonic()

    print(f"[{label}] Scanning for frames (sync header={sync_header.hex(' ')})...")

    while True:
        # Read whatever is available (up to 4 KB) to keep buffers drained.
        try:
            chunk = read_fn(4096)
        except Exception:
            chunk = b""
        if not chunk:
            return  # EOF / disconnect

        buf.extend(chunk)

        # Scan the buffer for complete, CRC-valid frames.
        while True:
            idx = buf.find(sync_header)
            if idx == -1:
                if len(buf) > 1:
                    junk_bytes += len(buf) - 1
                    del buf[: len(buf) - 1]
                break

            if idx > 0:
                junk_bytes += idx
                del buf[:idx]

            if len(buf) < frame_length:
                break  # wait for more data

            candidate = bytes(buf[:frame_length])
            if crc16_ccitt(candidate[:-2]) == int.from_bytes(candidate[-2:], "big"):
                del buf[:frame_length]

                # VC frame count gap detection (byte 3 of TM primary header).
                vc_count = candidate[3]
                if last_vc_count >= 0:
                    expected_vc = (last_vc_count + 1) & 0xFF
                    if vc_count != expected_vc:
                        gap = (vc_count - last_vc_count) & 0xFF
                        vc_frame_gaps += gap - 1
                        print(
                            f"[{label}] VC frame gap: expected {expected_vc}, "
                            f"got {vc_count} ({gap - 1} frame(s) lost)"
                        )
                last_vc_count = vc_count

                frames_found += 1
                yield candidate, vc_count
            else:
                # CRC mismatch — false positive sync header.
                del buf[:2]

        # Periodic stats.
        if stats_interval > 0:
            now = time.monotonic()
            if now - stats_time >= stats_interval:
                elapsed = now - stats_time
                rate = frames_found / elapsed if elapsed else 0
                print(
                    f"[{label}] stats: {frames_found} frames, {rate:.1f} f/s, "
                    f"{vc_frame_gaps} gap(s), {junk_bytes} junk bytes skipped"
                )
                stats_time = now
                frames_found = 0
                junk_bytes = 0

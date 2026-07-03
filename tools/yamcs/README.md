# YAMCS Adapter Tools

`proves_adapter.py` bridges the flight software UART and YAMCS over UDP. It forwards TM frames to YAMCS and wraps TC commands with the HMAC authentication header the FSW expects.

When you run `make yamcs UART_DEVICE=/dev/ttyXXX`, this adapter starts automatically in serial mode.

## Junk-byte capture (`data/`)

Serial TM uses a **HUNT / LOCK** sync state machine:

- **HUNT** — slide one byte at a time until a 248-byte window passes CRC-16/CCITT.
- **LOCK** — assume every 248 bytes is the next frame until CRC fails, then fall back to HUNT.

Bytes discarded during sync are logged to CSV for offline analysis. This helps answer: *what non-frame data is on the UART, and when does frame alignment break?*

Typical sources of junk bytes:

- FSW console text printed on the same serial port
- Noise or partial reads at startup
- A dropped or corrupted TM frame (shows up as `locked` in the log — sync was lost)

Files are written to `tools/yamcs/data/` as:

```text
junk_YYYY-MM-DD_HH-MM-SS.csv
```

One file is created per adapter session. Rows are appended immediately as bytes are discarded.

## CSV columns

| Column | Description |
| --- | --- |
| `timestamp` | Local time when the byte was discarded (`YYYY-MM-DD HH:MM:SS`) |
| `byte_index` | Position within the current HUNT run. Resets to `0` after a valid frame is found. |
| `hex_value` | The discarded byte, e.g. `0x44` |
| `state` | `hunt` while searching for sync; `locked` if the parser was in sync when this byte was dropped (CRC failed, sync lost) |

Console output uses the same values:

```text
[TM] junk | 2026-07-02 19:33:45 | idx=   3 | 0x44 | hunt
[TM] junk | 2026-07-02 19:33:52 | idx=   0 | 0xff | locked
```

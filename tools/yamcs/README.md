# YAMCS Adapter Tools

`proves_adapter.py` bridges the flight software UART and YAMCS over UDP. It forwards TM frames to YAMCS and wraps TC commands with the HMAC authentication header the FSW expects.

When you run `make yamcs UART_DEVICE=/dev/ttyXXX`, this adapter starts automatically in serial mode.

## TM capture (`data/`)

Serial TM uses a **HUNT / LOCK** sync state machine:

- **HUNT** — slide one byte at a time until a 248-byte window passes CRC-16/CCITT.
- **LOCK** — assume every 248 bytes is the next frame until CRC fails, then fall back to HUNT.

Every UART byte is logged for offline analysis: compare the full wire capture against valid packets and CRC-failed windows to debug sync loss, console text on the serial port, and frame alignment.

Each adapter run creates a timestamped session folder under `tools/yamcs/data/`:

```text
data/
  2026-07-02_20-13-45/
    stream.csv       # one row per byte, with classification state
    stream.hex       # full wire capture (16-byte hex rows)
    packets.hex      # valid 248-byte frames (raw UART bytes)
    dropped.hex      # every CRC-failed 248-byte window
```

The same byte events are also printed to the console.

## `stream.csv` columns

| Column | Description |
| --- | --- |
| `timestamp` | Local time when the byte was classified (`YYYY-MM-DD HH:MM:SS`) |
| `byte_index` | Session-global offset (0-based, matches position in `stream.hex`) |
| `hex_value` | The byte, e.g. `0x44` |
| `state` | `frame`, `hunt`, or `locked_discard` |

### States

| State | Meaning |
| --- | --- |
| `frame` | Byte is part of a valid CRC frame (also in `packets.hex`) |
| `hunt` | Byte was slid off after a CRC failure while not locked |
| `locked_discard` | Byte was slid off after a CRC failure while locked (sync lost) |

Bytes are classified when consumed by the sync machine: on CRC pass all 248 bytes become `frame`; on CRC fail the full window is written to `dropped.hex` and only the slid byte (`buf[0]`) is classified as `hunt` or `locked_discard`.

## Hex file formats

**`stream.hex`** — continuous capture, 16 bytes per line:

```text
0000: 1a 2b 3c 4d 5e 6f 70 81 92 a3 b4 c5 d6 e7 f8 09
0010: ...
```

**`packets.hex` / `dropped.hex`** — one block per 248-byte frame:

```text
--- packet 12 | offset=2960 | 2026-07-02 20:13:45 | SCID=68 VC=42 ---
0000: 1a 2b 3c ...
...
00f0: ...

--- dropped 8 | offset=1984 | 2026-07-02 20:13:46 | hunt ---
0000: ff 00 1a ...
...
```

`packets.hex` contains raw UART bytes (before SCID normalization). `dropped.hex` records every CRC-failed window; overlapping windows are expected during HUNT.

Console output mirrors `stream.csv`:

```text
[TM] byte | 2026-07-02 20:13:45 | idx=  2960 | 0x1a | frame
[TM] dropped | 2026-07-02 20:13:46 | offset=1984 | hunt | CRC failed
[TM] byte | 2026-07-02 20:13:46 | idx=  1984 | 0xff | hunt
```

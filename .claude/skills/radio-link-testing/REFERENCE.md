# Radio-link testing — reference

## Topology

```
GDS ──USB(21203, raw passthrough)──► GRC dataComDriver ──► byteComBridge ──► LoRa TX
                                                                                │
                                                                          (437.4 MHz)
                                                                                ▼
flight console (21101) ◄── F´ events   ComCcsdsLora.authenticationRouter ◄── LoRa RX
                                          │
                                          ├─► cmdDisp   (commands)
                                          └─► FileUplink (files)
```

- **GRC** = Ground Radio Controller (`Open-Source-Space-Foundation/ground-radio-controller`,
  RP2350/Zephyr/F´). Pure bridge: USB bytes -> LoRa, no reframing.
- **Flight board** = this repo's deployment. Single USB CDC `cu.usbmodem21101` is
  BOTH the Zephyr console AND the F´ `comDriver` UART. **No SWD/debug probe** — the
  console text is the only observability.
- The CMSIS-DAP probe (`cu.usbmodem102`) is on the **GRC**, not the flight board.

## Ports (numbers drift — re-verify each session)

| Port | Role |
|---|---|
| `cu.usbmodem21203` | GRC data port — GDS attaches here |
| `cu.usbmodem21201` | GRC Zephyr console — GRC-side events (`NoBuffsAvailable`, `UART buffer overrun`) |
| `cu.usbmodem21101` | Flight console + F´ comDriver — **the verdict** |
| `cu.usbmodem102` | CMSIS-DAP probe (on the GRC) |

`ls /dev/cu.usbmodem*` to list. Two boards share the bench — do not cross 21101 (flight)
with the GRC ports. If unsure, `ioreg -l` maps USB product strings to device nodes.

## Reading the verdict

F´ `CdhCore.textLogger` prints every event to the flight USB console as ASCII:
`[Os::Console] EVENT: (id) (time) SEVERITY: EvtName : msg`. The bytes are interleaved
with `0x44`-padded binary idle frames, so **always prefix greps with `LC_ALL=C`** and
match printable runs. Do NOT rely on a GDS frame-decode of 21101 — it misframes.

Key events: `NoOpReceived` (link alive), `OpCodeCompleted`/`OpCodeDispatched` (command ran),
`EmitSequenceNumber` (GET_SEQ_NUM reply), `FileReceived` (file complete), `BadChecksum`
(file complete but content gap from a lost frame), `UnexpectedSequenceCount` /
`PacketOutOfOrder` (dropped frames), `InvalidReceiveMode` (benign — stale transfer cleanup).

## Authentication (the real gate)

Uplink uses `authenticate-space-data-link` framing (`fprime-gds.yml`: frame-size 248,
file-uplink-chunk-size 204, cooldown 0.400). HMAC key default
`0x65b32a18e0c63a347b56e8ae6c51358a`. Each packet carries a sequence number; the flight
accepts seq in `[stored, stored+window(50000)]` and advances `stored=expected+1`.

- **Bypass opcodes** skip HMAC + seq, so they work regardless of sequence: `0x01000000`
  NO_OP, `0x2200b000` LoRa GET_SEQ_NUM, `0x10065000` TELL_JOKE. Use these as link tests.
- **Non-bypass** (everything else, incl. `lora.TRANSMIT` `0x1001f009`, and all FILE
  packets) needs GDS started at/ahead of the flight's current seq.
- **Sequence source:** GDS reads the starting seq from `Framing/src/sequence_number.bin`
  (ASCII int) at comm start and auto-increments per frame. Set it by writing that file
  BEFORE launching GDS. Keep it AHEAD of the board's stored seq (rejected frames bump the
  file but not the board). Window is huge (50000), so a value comfortably ahead is fine.
  Read the board's current seq via bypass GET_SEQ_NUM -> `EmitSequenceNumber` on console.
- The file uplinker synthesizes per-chunk handshakes LOCALLY (loopback), so file packets
  push regardless of flight TX state; only the *completion telemetry* needs flight TX.

## Why spray, and TX-disabled mode

The link is half-duplex. A single command often misses the flight's RX window, so commands
are **sprayed** (`SPRAY_N` sends at `SPRAY_GAP` spacing). Once flight TX is DISABLED
(`lora.TRANSMIT DISABLED`), the flight listens continuously and RX is near-100% reliable —
this is the user's standard uplink mode. With TX off there is no downlink, so GDS
`await_completion` times out (expected); judge by `FileReceived` on the console.

## Launching GDS

```bash
./fprime-venv/bin/fprime-gds --uart-device /dev/cu.usbmodem21203 \
    --uart-skip-port-check --gui none [--file-uplink-cooldown X] [--file-uplink-chunk-size N]
```
Wait for `/tmp/fprime-server-out`. Tear down any stale GDS first (duplicate
`CustomDataHandlers` on the same `/tmp/fprime-server-{in,out}` ipc cause duplicate/
out-of-order uplink packets). Use the venv binary — PATH lacks it in non-login shells.

## Gotchas

- `fprime-cli command-send` silently does NOT inject into a running headless GDS (exits 0,
  sends nothing). Use the pytest `fprime_test_api.send_command(...)` path.
- The file uplinker DELETES its source file on finish — always uplink a throwaway copy.
- Reflashing the GRC re-enumerates its USB → kills the 21201/21203 loggers AND breaks the
  GDS serial link on 21203 (the GDS process may stay alive but go deaf). Restart the
  console logger(s) and re-verify the link with a spray after any GRC reflash.
- `LC_ALL=C` before grepping the binary-laden console logs.
- No `timeout`/`gtimeout` on macOS by default.
- Loss is dominated by inter-frame SPACING, not RF margin — if frames drop, check the
  send rate before reaching for TX power. See `[[grc-uplink-crash]]` memory for the
  full link-tuning history.

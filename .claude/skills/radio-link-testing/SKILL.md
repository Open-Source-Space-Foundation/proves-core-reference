---
name: radio-link-testing
description: Test PROVES flight software end-to-end over the LoRa radio link (GDS -> GRC bridge -> flight board) on the bench rig. Use when testing command or file uplink over the radio, exercising the GRC LoRa bridge, validating the GRC-to-flight link, or debugging why uplinked commands/files don't reach the flight board.
---

# Radio-link testing (GDS -> GRC bridge -> LoRa -> flight)

Drive the flight software over the real LoRa link the way an operator would: a
headless GDS pushes authenticated commands/files into the **GRC** (Ground Radio
Controller) over USB; the GRC bridges them to LoRa; the **flight board** receives
them. The flight board has **no debug probe** — the only verdict is the text it
prints on its USB console. See [REFERENCE.md](REFERENCE.md) for topology, ports,
auth internals, and gotchas. Run everything from the flight repo root.

## Rig at a glance

- `cu.usbmodem21203` = GRC data port (GDS attaches here; raw passthrough to LoRa)
- `cu.usbmodem21201` = GRC Zephyr console (GRC-side events: NoBuffs, overruns)
- `cu.usbmodem21101` = flight board console **= the verdict** (F´ events as ASCII)
- Port numbers drift — verify with `ls /dev/cu.usbmodem*`; two boards are on the bench.

## Quick start

```bash
# 1. Launch headless GDS on the GRC data port (leave running)
./fprime-venv/bin/fprime-gds --uart-device /dev/cu.usbmodem21203 \
    --uart-skip-port-check --gui none &        # wait for /tmp/fprime-server-out

# 2. Log the flight console (the only observability) in the background
./fprime-venv/bin/python3 .claude/skills/radio-link-testing/scripts/console_logger.py \
    /dev/cu.usbmodem21101 /tmp/flight-console.log &

# 3. Sanity check the link: an authenticated NO_OP must round-trip
PYTEST=( ./fprime-venv/bin/pytest -s -o addopts=""
  --dictionary build-artifacts/zephyr/fprime-zephyr-deployment/dict/ReferenceDeploymentTopologyDictionary.json )
"${PYTEST[@]}" PROVESFlightControllerReference/test/int/bridge_uplink_test.py::test_noop
# verdict: `NoOpReceived` appears in /tmp/flight-console.log
```

## Workflow: uplink a file

The user's operational mode is **flight TX disabled** during uplink (avoids
half-duplex contention). Then GDS gets no downlink, so completion telemetry never
returns — `uplink_file_and_await_completion` **times out, which is expected**. The
only success signal is `FileReceived` on the flight console.

1. **Disable flight TX** (spray — single sends miss the half-duplex RX window):
   `TX_STATE=DISABLED SPRAY_N=12 "${PYTEST[@]}" ...::test_spray_disable_tx`
   Confirm ~all `OpCodeCompleted` (opcode `0x1001f009`) on the flight console.
2. **Copy the source first** — the uplinker DELETES its source on finish:
   `cp myfile.bin /tmp/up.bin`
3. **Uplink:** `UPLINK_SRC=/tmp/up.bin UPLINK_DEST=//dest.bin UPLINK_TIMEOUT=70 "${PYTEST[@]}" ...::test_uplink_file`
4. **Read the verdict** on `/tmp/flight-console.log` (prefix greps with `LC_ALL=C`):
   - `FileReceived : Received file //dest.bin` + no `BadChecksum` = clean win.
   - `BadChecksum` = file arrived but a frame was lost over-air (no retransmit in TX-off mode).
   - Check the GRC console for `NoBuffsAvailable` / `UART buffer overrun` = GRC-side drops.

## Test driver

`PROVESFlightControllerReference/test/int/bridge_uplink_test.py` — tests:
`test_noop`, `test_get_seq`, `test_spray_seq`, `test_spray_disable_tx`
(env `TX_STATE`/`SPRAY_N`/`SPRAY_GAP`), `test_enable_tx`, `test_uplink_file`
(env `UPLINK_SRC`/`UPLINK_DEST`/`UPLINK_TIMEOUT`), `test_calc_crc`
(env `CRC_FILE` → on-board `CalculateCrc`, the reliable read-back oracle),
`test_detect_drops` (env `DETECT_FILE`/`DETECT_PKT` → `DETECT_DROPS`; **unreliable, see
OTA doc**). Always run with the explicit `--dictionary` above and the venv pytest.
`fprime-cli command-send` does NOT inject into a running headless GDS — only the pytest
`send_command` path transmits.

## Reliable OTA file delivery over the lossy link

To deliver a file intact despite over-air frame loss (and verify it without downlinking),
see **[OTA-DROP-PATCHING.md](OTA-DROP-PATCHING.md)**: re-uplink the whole file to the same
dest until `fileManager.CalculateCrc` matches the truth CRC (`= zlib.crc32 ^ 0xFFFFFFFF`).
`UPLINK_TIMEOUT` must exceed the real transfer time (~7 min for 100 KB @0.4 s) or the
transfer aborts mid-stream. Do not trust `DETECT_DROPS` (non-deterministic false positives).

For the bandwidth-efficient, pass-window-aware version (split into parts, re-upload only the
dropped parts, reassemble with `AppendFile`), see
**[OTA-TIER1-PERFORMANCE.md](OTA-TIER1-PERFORMANCE.md)** and the validated driver
`scripts/tier1-patch.sh <source> <num-parts>`. Measured uplink ≈ 256 B/s (~0.8 s/chunk,
SF8); an update spans multiple ~4.5-min passes — dropping to SF7 near closest approach ≈
doubles throughput. `AppendFile` is NOT idempotent — send it once per part, never sprayed.

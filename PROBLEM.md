# CI hardware failure on hmac-to-storage (PR #472): board never responds over UART/LoRa

## Symptom

`integration-uart` and `integration-radio` both fail at the very first test
(`provision_key_test.py::test_provision_key`), inside the `start_gds` fixture's
`CMD_NO_OP` retry loop. GDS never sees any reply:

- `recv.bin` is 0 bytes for the whole test window (one run showed 48 bytes — see below).
- `sent.bin` is 0 bytes: the ground side's write to the serial device itself is failing.
- `comm.py.log` shows real OS-level serial errors, not just logical timeouts:
  ```
  [WARNING] serial_adapter: Serial exception caught: device reports readiness to
    read but returned no data (device disconnected or multiple access on port?).
    Reconnecting.
  [WARNING] uplink: Uplink failed to send 41 bytes of data after 3 retries
  ```
- This reproduced identically on two different physical benches (UART bench and
  LoRa/radio bench), ruling out a single flaky cable/bench.
- `lint`, `unit-test`, `build`, `yamcs-build` are all green. Only the two
  hardware-integration jobs fail.

## Investigation

Three rounds of live SWD diagnostics were run against the UART bench (via a
temporary CI step in `.github/workflows/ci.yaml`, since removed — see commits
`1ff5d8bb`, `78ca10f7`, `0949dfbd`, `693eb5d8` on this branch for the added/fixed/
removed diagnostic). Each halted both RP2350 cores ~20s after boot via OpenOCD and
dumped registers + CFSR/HFSR/DFSR/MMFAR/BFAR (`0xE000ED28`).

Attempt 1: broken OpenOCD command syntax (`rp2350.cm0 halt` isn't valid — only
`arp_halt` exists per-target-instance); OpenOCD printed a command-usage listing and
exited before reaching any read. No data.

Attempt 2: fixed target selection (`targets rp2350.cm0` then plain `halt`/`reg`), but
hit two problems: OpenOCD warned `target was in unknown state when halt was
requested` (a race between `halt` and its own initial poll cycle) so the bulk `reg`
dump came back with register **names** but no **values**; and `bt` (not a valid
OpenOCD console command — that's GDB) aborted the remaining `-c` chain before core1
was ever queried. Partial data: CFSR=0, HFSR=0, DFSR=1 for core0 (a real debug-halt,
no fault).

Attempt 3: added explicit `poll` before/after `halt`, a settle delay, and queried
`pc`/`lr`/`sp`/`xpsr`/`r0`-`r3` individually instead of the bulk dump; dropped `bt`.
This got real data for both cores:

```
cm0: pc=0x101864b8 lr=0x1010fb69 sp=0x20034410 xpsr=0x61000000 (Thread mode, no fault)
     CFSR=0 HFSR=0 DFSR=0
cm1: pc=0x0000019e lr=0x00000203 sp=0xf0000000 xpsr=0x09000000
     CFSR=0 HFSR=0 DFSR=1 (plain debug halt)
```

Resolved against a local build of the same commit's `zephyr.elf`
(`build-artifacts/zephyr.elf`, built by a prior session — see TODO.md):

```
$ arm-none-eabi-addr2line -e build-artifacts/zephyr.elf -f -C 0x101864b8 0x1010fb69
fs_open
lib/zephyr-workspace/zephyr/subsys/fs/fs.c:140
idle
lib/zephyr-workspace/zephyr/kernel/idle.c:30
```

**Core0 (the only core Zephyr actually runs on this board) was live, in Thread
mode, with zero fault flags set, executing `fs_open()` at the moment it was
halted.** Core1 is just idling in the RP2350 boot ROM waiting for a multicore
launch that never comes (expected/benign for a single-core app).

This rules out a crash, a hard fault, a null pointer dereference reaching a fault
handler, and a boot-time panic. The board is alive and running normal application
code; it is *not* in a fault-loop.

## Leading hypothesis (not yet confirmed)

This PR (per the plan) moves two things onto a new internal-flash littlefs
partition (`keystore_partition`, mounted at `/keys`) that previously lived on the
SD-card FAT filesystem:

1. The anti-replay sequence number (`SEQ_NUM_FILE_PATH` default now
   `/keys/sequence_number.bin`, was on `/` = SD/FAT).
2. The new HMAC key store (`/keys/authkeys.bin`), which `TcSecurityDeframer`
   reloads from disk once whenever an incoming frame's SPI doesn't match any
   active slot — i.e., on **every single received frame** while the board is
   unprovisioned (keyless), which is exactly the state under test here.

Both files now live on the **same physical QSPI/XIP flash chip that serves the
running firmware code**. On RP2040/RP2350, writing or erasing that flash requires
suspending code execution from flash and disabling interrupts system-wide for the
duration of the operation (a well-documented Pico SDK / Zephyr internal-flash
driver constraint — this is *not* true of the old SD-card/SDMMC path, which uses a
separate SPI bus with no such restriction).

`TcSecurityDeframer::dataIn_handler` (see
`PROVESFlightControllerReference/Components/TcSecurityDeframer/TcSecurityDeframer.cpp`,
around lines 56–119) holds both `m_keyStoreLock` and `m_sequenceNumberLock` for the
full validate+authenticate+persist sequence on every frame, and:
- writes the sequence-number file (`writeSequenceNumber`) on every *accepted*
  frame, and
- calls `loadKeyStore()` (a fresh `fs_open`+read) on every frame with an
  unrecognized SPI, before this PR happens to be *every* frame during the keyless
  window this test exercises.

The working theory is that this internal-flash I/O — now on a hot path that used
to be interrupt-safe SD-card I/O — is disabling interrupts long enough, and/or
often enough, to stall USB CDC-ACM servicing, producing exactly the "device
disconnected" / failed-write symptom seen on the ground side. The single mid-flight
`fs_open` sample is consistent with this but is not proof by itself (no flash-
operation timing instrumentation was captured); a controlled repro (e.g.
instrumenting write duration with a GPIO toggle or RTT, or scoping bus activity)
would be needed to confirm the magnitude and pin down which specific call
(`writeSequenceNumber` vs `loadKeyStore` vs the very first-ever littlefs auto-format
at boot) is responsible.

## What this is NOT

- Not a build/compile problem — `build`, `unit-test`, `lint`, `yamcs-build` are all
  green.
- Not a crash/fault/panic — confirmed via live register + CFSR/HFSR read on real
  hardware, core0 healthy and running.
- Not bench/cable flakiness alone — reproduced identically on two separate physical
  benches (UART and LoRa).
- Not the CI runner being stuck — earlier apparent multi-hour queue delays during
  this investigation turned out to be real backlog on the shared hardware runner,
  unrelated to this bug; runs did eventually execute and complete.

## Suggested next steps (not yet implemented — needs a decision)

The sequence-number-and-key-store-onto-internal-flash design is a locked decision
from the original plan (`~/.claude/plans/quirky-forging-possum.md`), so the fix
should stay within that design rather than reverting to SD-card storage. Candidates
worth evaluating:

1. Debounce/throttle `writeSequenceNumber` so it isn't a synchronous internal-flash
   write on every single accepted frame — e.g. only persist every N frames or after
   a time interval, accepting a small anti-replay window on power loss (arguably
   already implicitly tolerated by the existing `SEQ_NUM_WINDOW` mechanism).
2. Stop calling `loadKeyStore()` on *every* unrecognized-SPI frame; e.g. rate-limit
   reloads (the plan's stated purpose — cross-link key-rotation propagation — is an
   infrequent event, not something that needs a fresh disk read on every single
   uplinked frame while unprovisioned).
3. Confirm whether the very first-ever littlefs mount on a blank `keystore_partition`
   performs a full-partition format synchronously at `SYS_INIT` (before `main()`),
   and if so, whether that's the dominant one-time stall rather than (or in addition
   to) the per-frame writes.
4. Instrument actual flash-operation duration on real hardware (GPIO toggle around
   `flash_area_write`/`erase`, or Zephyr's flash driver trace hooks) to know the real
   magnitude before choosing a fix, rather than guessing further.

None of the above has been implemented yet — see `TODO.md` for the standing
checklist item.

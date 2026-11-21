# Components::PsramMonitor

Passive F´ component that monitors the RP2350 QMI PSRAM (APMemory APS1604M,
issue #285, slice S6).

## Purpose

Provides ground-visible telemetry of PSRAM init status and size at 1 Hz,
and an on-command bounded memtest for in-flight health verification.

## Telemetry

| Name | Type | Description |
|---|---|---|
| PsramSize | U32 | PSRAM region size in bytes (0 if absent/disabled) |
| PsramStatus | U8 | 0=absent/init-failed, 1=mapped, 2=heap-verified |

### PsramFree — rationale for omission

Zephyr v4.3 `shared_multi_heap` has no "free bytes" query API.  An atomic
counter maintained outside the driver is impossible without intercepting
every alloc/free call.  A binary-search probe-alloc/free approach (find
largest allocatable block, ≤20 iterations) would work but is only
appropriate during the self-test command, not on every 1 Hz tick because:

- Repeated small probe allocations fragment the heap.
- alloc/free calls are not ISR-safe and must not run in a rate-group
  handler that may preempt other tasks.

**Decision:** `PsramFree` is omitted from rate-group telemetry.  A future
iteration may add it to the `PSRAM_SELF_TEST` response if operators need
a free-space estimate.

## Commands

| Name | Arguments | Description |
|---|---|---|
| PSRAM_SELF_TEST | start_offset: U32, length: U32 | Bounded memtest over [base+start_offset, +length) |

### Self-test details

- **Patterns:** word-wise save/test/restore — each 32-bit word is saved,
  written with the address-in-address pattern (`i`) and its inverse (`~i`)
  with read-back checks, then the original value is restored.
- **Non-destructive at rest:** heap contents in the tested region are
  preserved.  A concurrent writer to the same word can still race the
  one-word test window, so prefer quiescent regions.  Full-region
  *destructive* address-decode coverage is provided by the driver's boot
  memtest (`CONFIG_MEMC_RP2350_PSRAM_SELF_TEST`), which runs before the
  heap exists.
- **Alias:** test runs through the uncached, non-allocating XIP CS1 alias
  (`psram_nocache_base()`, `0x15000000` on RP2350) so every access hits
  the chip rather than the write-back XIP cache.
- **Validation:** `start_offset` must be 4-byte aligned; `length` is
  rounded down to whole words and clamped to the region.
- **Length cap:** `256 KB` per invocation (`SELF_TEST_LENGTH_CAP`).  This
  keeps the blocking time inside the command dispatcher under ~10 ms.
  For a full 2 MB test, issue 8 consecutive commands with `start_offset`
  stepping by 262144 (0x40000).
- **Safety:** the command is rejected immediately (with a `PsramNotReady`
  event) if `psram_is_ready()` returns false.

## Events

| Name | Severity | Description |
|---|---|---|
| PsramSelfTestPassed | ACTIVITY_LO | Self-test passed for given offset+length |
| PsramSelfTestFailed | WARNING_HI | First mismatch: offset, expected, actual |
| PsramNotReady | WARNING_LO | Command rejected — PSRAM not ready |

## Rate Group

Connected to `rateGroup1Hz` (1 Hz) — the slowest rate group in the
ReferenceDeployment topology.  Cheap reads only on the rate-group path
(`psram_is_ready()`, `psram_heap_ok()`, `psram_size()`): these are
lock-free boolean/size queries against driver state set once at boot.

## Stub behaviour (boards without PSRAM)

All Zephyr driver calls are guarded by `#ifdef CONFIG_MEMC_RP2350_PSRAM`.
On boards where the PSRAM node is `status = "disabled"` (v5e, v5, v5c),
the component compiles and runs normally, reporting:

- `PsramSize = 0`
- `PsramStatus = 0` (absent)
- `PSRAM_SELF_TEST` → `PsramNotReady` event + EXECUTION_ERROR response

## SWD-readable confirmation

The component writes `PsramStatus` and `PsramSize` telemetry every second.
To confirm the component ran without a GDS connection, read the F´ channel
buffer via SWD/OpenOCD after ≥2 seconds of boot.  The Zephyr driver also
logs `psram: ready — 2 MB at 0x11000000` at `LOG_INF` level on boot.

## Change Log

| Date | Description |
|---|---|
| 2026-07-11 | Initial implementation — S6 of issue #285 |

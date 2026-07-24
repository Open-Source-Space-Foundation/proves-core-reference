# Investigation: hmac-to-storage CI hardware failure — root-cause re-analysis

Follow-up to `PROBLEM.md`. This document records a static re-analysis of the
`integration-uart` / `integration-radio` failure on branch `hmac-to-storage`
(PR #472), tracing the device tree, the RP2350 flash driver, the littlefs
automount path, and the `TcSecurityDeframer` hot path.

**Headline conclusion:** the "internal-flash writes disable interrupts and stall
USB" hypothesis in `PROBLEM.md` is very likely a phantom. The keystore littlefs
partition is placed **beyond the declared flash boundary**, so every keystore
flash operation is rejected with `-EINVAL` *before* any interrupt is disabled,
and `/keys` can never mount. The most probable real fix is a one-line flash-size
correction in the device tree.

Confidence: the partition-out-of-bounds and read-vs-write facts below are
**code-confirmed**. Whether the mount failure is the *sole* cause of the USB
symptom is **not yet confirmed on hardware** — see "Next steps".

---

## Finding 1 — Reads do NOT disable interrupts (contradicts PROBLEM.md hypothesis #2)

`lib/zephyr-workspace/zephyr/drivers/flash/flash_rpi_pico.c`:

- `flash_rpi_read` (line 46) is a bare `memcpy` from the XIP-mapped flash window.
  **No `irq_lock`.**
- Only `flash_rpi_write` (line 82) and `flash_rpi_erase` (line 132) take
  `irq_lock()` / `irq_unlock()`.

Therefore `loadKeyStore()` — a pure `fs_open`+read invoked on every
unrecognized-SPI frame (`TcSecurityDeframer.cpp:70`) — **cannot** stall USB via
interrupt disable. The mid-flight `fs_open` PC sample that `PROBLEM.md` treats as
its strongest evidence (`cm0: pc=0x101864b8 → fs_open`) is a *harmless read*, not
a stall. `PROBLEM.md` item #2 (per-frame reload) is a non-issue for USB timing.

## Finding 2 — `keystore_partition` is placed past the end of the declared flash

`boards/bronco_space/proves_flight_control_board_v5/proves_flight_control_board_v5.dtsi:77`:

```
&flash0 { reg = <0x10000000 DT_SIZE_M(4)>; }     /* 4 MB */
```
Confirmed in the generated build: `CONFIG_FLASH_SIZE=4096`,
`CONFIG_FLASH_BASE_ADDRESS=0x10000000`
(`build-fprime-automatic-zephyr/zephyr/.config`).

Partition map (`.dtsi` / generated `zephyr.dts`):

| partition            | offset     | end        | note                         |
|----------------------|------------|------------|------------------------------|
| boot (mcuboot)       | 0x000000   | 0x100000   |                              |
| slot0 (current)      | 0x100000   | 0x200000   |                              |
| slot1 (golden)       | 0x200000   | 0x300000   |                              |
| slot2 (test)         | 0x300000   | 0x400000   | fills the entire 4 MB        |
| **keystore_partition** | **0x400000** | 0x440000 | **starts AT the 4 MB end**   |
| storage_partition    | 0x440000   | 0x1000000  | runs to 16 MB                |

`keystore_partition@0x400000` begins exactly at the declared flash boundary and
is entirely out of bounds. The flash driver gate:

```c
#define FLASH_SIZE KB(CONFIG_FLASH_SIZE)        /* = 4 MB = 0x400000 */
static bool is_valid_range(off_t offset, uint32_t size) {
    return (offset >= 0) && ((offset + size) <= FLASH_SIZE);
}
```

`flash_area_*` passes the partition's flash-relative offset (`0x400000`) to the
driver, so `is_valid_range(0x400000, size)` → `(0x400000 + size) <= 0x400000` →
**false → `-EINVAL`**. This check runs **before** the `irq_lock` in both
`flash_rpi_write` and `flash_rpi_erase`.

Consequences:

1. `lfs_mount` → format (`littlefs_fs.c:966–971`) → `erase` → `-EINVAL` → format
   fails → **`/keys` never mounts.** The feature as shipped cannot persist keys
   or the sequence number at all.
2. Since no keystore flash op ever executes, **no interrupt is ever disabled for
   it** — the interrupt-stall mechanism in `PROBLEM.md` has nothing to act on.

## Finding 3 — This is a latent flash-size mis-declaration, newly exposed

On `main`, `storage_partition` already ran `0x400000 → 0x1000000` (12 MB at
offset 4 MB) against the same 4 MB `flash0` declaration (confirmed via
`git diff main..hmac-to-storage` on the `.dtsi`). It never mattered because
**nothing mounted `storage_partition`** — the SD card (FAT, `disk-access`) serves
`/`, and that is the only fstab mount on `main`.

This branch is the first to actually *mount* a filesystem at an offset ≥ 4 MB
(littlefs at `/keys`), which is what exposed the pre-existing mismatch. The 16 MB
extent of `storage_partition` strongly implies the physical chip is a 16 MB
W25Q128-class part and the `DT_SIZE_M(4)` declaration is simply wrong.

## What still needs hardware confirmation

The mount failure alone would normally degrade gracefully (the deframer tolerates
a missing store / keyless boot), so it is not yet proven that it is the *sole*
cause of the observed USB "device disconnected" symptom. It is, however, a
definite defect that makes the feature non-functional and that removes the basis
for the interrupt-stall theory. Confirm on hardware before ruling other causes
in or out.

---

## Storage layout for v5e (as built)

- `zephyr,flash = &flash0` — internal QSPI/XIP flash, declared 4 MB (**suspected
  under-declaration**; see Finding 3).
- fstab has two mounts:
  - `ffs1` — **FAT on the SD card** (spi0 / sdmmc-disk, mount `/`, `disk-access`).
    Separate SPI bus, no XIP interrupt-disable constraint. This is where the
    sequence number lived before this branch.
  - `lfs1` — **littlefs on `keystore_partition`** (mount `/keys`, `automount`) —
    added by this branch; currently unmountable (Finding 2).
- USB CDC-ACM (`CONFIG_USB_DEVICE_STACK_NEXT`, v5e defconfig) is the GDS
  transport; the "device disconnected" errors are on this link.

---

## Fix applied

No hardware bench was available this session to do the SWD-read / console-log
confirmation described below as the original "Step 0". Proceeded on the
strength of the existing evidence instead (Finding 3: `storage_partition`
already ran to 16 MB against this same 4 MB declaration on `main`, unnoticed
only because nothing mounted it).

Change made: `&flash0 { reg = <0x10000000 DT_SIZE_M(4)>; }` →
`DT_SIZE_M(16)` in `proves_flight_control_board_v5.dtsi` (shared by the v5c/v5d/v5e
board variants via `#include`). `make generate build` confirms `CONFIG_FLASH_SIZE`
now follows to 16384 (was 4096) with an otherwise identical, clean build (FLASH
69.82%, RAM 62.32% — unchanged from before the edit).

**Confirmed on hardware (CI run 30034861047, 2026-07-23):** `Flash Firmware`
step's OpenOCD output reports `RP2350 rev 3, QSPI Flash win w25q128fv/jv id =
0x1840ef size = 16384 KiB in 4096 sectors` — the chip really is 16 MB, so the
`.dtsi` fix is correct and `CONFIG_FLASH_SIZE`/`is_valid_range` now permit the
`keystore_partition` range.

**But `integration-uart`/`integration-radio` still fail identically** — GDS's
`comm.py.log` shows the same repeated
`Serial exception caught: device reports readiness to read but returned no
data (device disconnected or multiple access on port?). Reconnecting.`
starting ~19s after GDS start, and `start_gds`'s `CMD_NO_OP` never gets a
response within the whole 30s test window (both `provision_key_test.py` and
`format_filesystem_test.py` fail the same way in `integration-uart`; the radio
job fails at the identical `Bootstrap Sequence Number over UART` /
`provision_key` step). So the flash-size mis-declaration was real and worth
fixing, but it was **not the sole cause** of the CI failure.

## Revised theory: PROBLEM.md's interrupt-stall theory may now apply for real

Before this fix, every keystore flash op was rejected by `-EINVAL` *before*
`flash_rpi_write`/`flash_rpi_erase` ever reached `irq_lock()` — that's why
Finding 1/2 above concluded the interrupt-stall theory had "nothing to act on."
Now that `/keys` can actually mount, `lfs_mount`'s first-ever format on this
partition (superblock write, at minimum) is the first code in this branch that
actually reaches `flash_rpi_erase`/`flash_rpi_write`, both of which wrap the
*entire* erase/program call in `irq_lock()`/`irq_unlock()`
(`flash_rpi_pico.c:132-136`, `:82-107`) — with **no yielding** for however long
`flash_range_erase`/`flash_range_program` (Pico SDK bootrom calls) take. If
that takes long enough, USB CDC-ACM polling stalls exactly like the
symptom shows. This would mean the size fix was a necessary but not
sufficient change — PROBLEM.md's mechanism is real, it just couldn't fire
until the partition became mountable.

**Not yet confirmed**: whether it's actually the littlefs format path (one-time,
at first boot on a virgin partition) vs. per-frame `loadKeyStore()`/
`writeSequenceNumber` reloads causing repeated stalls throughout the test. The
timing (~19s in, then continuing through the whole 30s window) is at least
consistent with either. Console/log output is disabled in these builds
(`make check-console-disabled` is a required check), so there's no boot log
visibility in this CI run to distinguish the two — a temporary
`CONFIG_LOG=y` diagnostic CI run (same pattern as the prior fault-register
diagnostic commits `1ff5d8bb`/`78ca10f7`/`0949dfbd`/`693eb5d8`) would show
whether the stall is at first-mount/format or ongoing.

## Console-log diagnostic (CI run 30036653676, 2026-07-23, reverted)

Temporarily enabled `CONFIG_CONSOLE`/`CONFIG_UART_CONSOLE`/`CONFIG_PRINTK`/
`CONFIG_LOG` and captured raw serial from a fresh power-cycle for 40s before
GDS ever opened the port (console shares `cdc_acm_uart0` with the F' downlink
per `scripts/check_console_disabled.py`, so this intentionally desynced GDS
for the run — same tradeoff as the prior fault-register diagnostics).

Captured log (`console-boot.log`, full contents):

```
[00:00:00.034,000] <inf> LSM6DSO: Initialize device LSM6DSO
[00:00:00.035,000] <inf> LSM6DSO: chip id 0x6c
[00:00:00.785,000] <inf> sd: Maximum SD clock is under 25MHz, using clock of 24000000Hz
*** Booting Zephyr OS build v4.4.1 ***
[00:00:00.795,000] <inf> usbd_init: bNumInterfaces 2 wTotalLength 75
[00:00:00.927,000] <inf> usbd_core: Actual device speed 1
[00:00:01.040,000] <inf> usbd_core: Actual device speed 1
[00:00:01.162,000] <inf> usbd_ch9: protocol error: (GET_DESCRIPTOR/DEVICE_QUALIFIER, "not supported" — routine/benign for a full-speed-only device, seen twice)
```

...then **nothing** for the remaining ~39s of the 40s capture window — no
further log lines, and critically no raw TM-frame bytes either (F' emits
periodic telemetry within the first second or two of a normal boot; even
console-corrupted binary noise from that would show up as bytes in the
capture). Ruled out one specific hypothesis: the Pico SDK's own
`flash_range_erase()` has a separate `hard_assert(flash_offs + count <=
PICO_FLASH_SIZE_BYTES)` guard (`lib/zephyr-workspace/modules/hal/rpi_pico/src/
rp2_common/hardware_flash/flash.c`) independent of Zephyr's `CONFIG_FLASH_SIZE`
— but `PICO_FLASH_SIZE_BYTES` is not defined anywhere in this Zephyr build
(confirmed via grep across `lib/zephyr-workspace/zephyr/` and the generated
build's compile flags), so that `#ifdef`-guarded assert is compiled out
entirely and cannot be firing.

**Reading**: total silence this early (before first telemetry) is consistent
with a full system hang very early in boot — plausibly right around where the
`/keys` fstab automount would run — but the console log alone doesn't prove
*where*. It could be `irq_lock()` held for a very long single erase/program
call (the "revised theory" above), or something else entirely blocking
further interrupt/scheduler activity. Diagnostic reverted (prj.conf +
ci.yaml back to pre-diagnostic state) since it can't safely coexist with a
working GDS link.

**Next diagnostic (not yet run)**: repeat the original fault-register/SWD
approach (commits `1ff5d8bb` etc.) but at multiple time offsets *after* a
fresh flash — e.g. halt+dump PC at t=+2s, +10s, +20s — to see whether PC is
parked inside `flash_range_erase`/`flash_range_program`/the bootrom routines
they call for an extended period. That would confirm or rule out the
interrupt-stall theory directly without touching console/USB at all, avoiding
the corruption tradeoff entirely.

## PC-sweep diagnostic (CI run 30043983799, 2026-07-23) — hang located

Ran the sweep above (`PC Sweep Diagnostic` step, halt/resume only, no
firmware/config changes). Result for `cm0` (the core running Zephyr/F'):

| offset | pc         | lr         | sp         | xpsr       |
|--------|------------|------------|------------|------------|
| t=2s   | 0x101864b8 | 0x1010fb69 | 0x20034410 | 0x61000000 |
| t=10s  | 0x101864b8 | 0x1010fb69 | 0x20034410 | 0x61000000 |
| t=20s  | 0x101864b8 | 0x1010fb69 | 0x20034410 | 0x61000000 |

**Identical PC/LR/SP/xPSR at all three offsets spanning 20 seconds** — cm0
made zero forward progress the entire window. Resolved against this exact
commit's local build (`build-fprime-automatic-zephyr/zephyr/zephyr.elf`,
same `f5c3a124` the CI run built from):

```
101864b6 T fs_open
101864b6 T fs_open        <- pc 0x101864b8 is fs_open+2 (its first real instruction)
1010fb48 T idle
1010fb90 t unpend_thread_no_timeout   <- lr 0x1010fb69 is idle()+0x21 (its caller)
```

So cm0 is parked at the very entry of `fs_open()`, called from Zephyr's early
init sequence (which runs in the context that becomes the idle thread before
the scheduler starts other threads — this is the boot-time `SYS_INIT`/fstab
automount call chain, not literally the CPU-idle loop). This is consistent
with the **first real file operation this branch performs after a successful
`/keys` mount** — almost certainly `TcSecurityDeframer::configure()` calling
`loadKeyStore()`'s `fs_open()` on a virgin key-store file, i.e. exactly the
code path the flash-size fix newly unblocked.

`cm1` (the second RP2350 core) sampled `pc=0x19e`/`msp=0xf0000000` unchanged
at all three offsets too — `0x19e` is a bootrom address (well below the
`0x10000000` XIP flash base), meaning **cm1 was never launched into Zephyr
code at all** and has been idling in the bootrom's core-1 launch stub since
reset. This app apparently runs single-core (cm1 unused/unlaunched).

This is a real, reproducible, total hang (not a slow operation) at the exact
point the new keystore feature first touches the filesystem, and it fully
explains the console-log diagnostic's total silence after ~1.16s. It doesn't
by itself prove the *mechanism* (why `fs_open`/whatever it calls never
returns), but two mechanisms fit the facts and are worth checking first:

1. **RP2350 dual-core flash lockout deadlock**: the vendored
   `flash_range_erase`/`flash_range_program` in
   `lib/zephyr-workspace/modules/hal/rpi_pico/src/rp2_common/hardware_flash/flash.c`
   call `flash_exit_xip_func()`/ROM erase-program routines directly — no
   `multicore_lockout`/`flash_safe_execute` handshake is visible in this
   vendored copy, so a genuine multicore lockout wait is less likely here
   than on stock Pico SDK, but worth double-checking the ROM functions
   themselves don't internally expect core1's cooperation given cm1 was never
   launched.
2. **A global fs/littlefs lock held by a different, already-wedged context**:
   if some earlier code path (e.g. an interrupt-context or another thread)
   is genuinely stuck inside a flash erase/program with `irq_lock()` held
   indefinitely, `fs_open()`'s first action (typically taking a shared fs
   mutex) would block forever waiting for a lock that will never be
   released — cm0's halted PC/LR here would be showing the *this* thread's
   blocked-on-mutex state, not literally an infinite loop inside `fs_open`
   itself. Under this reading the real hang is still likely to be an
   erase/program call somewhere in the mount/format path, just not the one
   `fs_open` is calling right now.

Both point at the same practical fix: avoid littlefs's first-time
format/create path on this partition. The pre-existing "Robustness
follow-ups" item — replacing the littlefs `/keys` mount with raw
`flash_area_*`/NVS for this fixed-size key store — sidesteps this class of
bug entirely regardless of which exact mechanism is at fault, since NVS
doesn't take a global fs lock and its record writes are simple, bounded
`flash_area_write`/`erase` calls with well-understood timing.

One thing that's **ruled out** as the mechanism: a stale
`PICO_FLASH_SIZE_BYTES` hard_assert in the Pico SDK's
`flash_range_erase`/`flash_range_program` (`hard_assert(flash_offs + count <=
PICO_FLASH_SIZE_BYTES)`) — that macro is not defined anywhere in this Zephyr
build (confirmed via grep across `lib/zephyr-workspace/zephyr/` and the
generated build's compile flags), so the `#ifdef`-guarded assert is compiled
out entirely and cannot be firing.

### Recommended fix

> **Superseded / caveated by the SWD register forensics below (runs
> 30051402310 + 30052303346).** Those runs **ruled out** a wedged flash/XIP
> interface — the scenario in which switching to `flash_area_*`/NVS would have
> been wasted effort — but they also showed the hang is a *software*
> control-flow failure (interrupts masked via `PRIMASK`, an off-boundary PC,
> starved IRQ), not a clean littlefs mutex block. So "avoid the littlefs
> format path" is no longer established as *the* fix; the exact software cause
> is still being pinned (GDB backtrace, v3). Read the forensics section before
> acting on the recommendation below.

Regardless of which exact mechanism is at fault, both candidates above point
at the same remedy: **avoid littlefs's mount/format/first-file-create path on
this partition entirely.** Replace the `/keys` littlefs mount with raw
`flash_area_*` calls or the Zephyr NVS backend for this fixed-size key store +
sequence-number counter. This is a nontrivial rework of
`TcSecurityDeframer`'s `loadKeyStore()`/`writeKeyStore()`/`writeSequenceNumber()`
(currently built on Zephyr's `fs_open`/`fs_read`/`fs_write` POSIX-ish file
API) to instead use `flash_area_open`/`flash_area_read`/`flash_area_write`/
`flash_area_erase` directly against `keystore_partition`, or to adopt the NVS
subsystem's key-value API instead. Either avoids the littlefs mount/format
path entirely. **Not yet implemented.**

### Diagnostic history (all reverted except the flash-size fix; CI is currently back to pre-diagnostic state otherwise)

| run | change | result | reverted commit |
|-----|--------|--------|------------------|
| 30026433728 | (baseline, pre-fix) | fail, same USB-disconnect symptom | — |
| 30034861047 | flash0 `DT_SIZE_M(4)` → `DT_SIZE_M(16)` (kept, not reverted) | fail, same symptom; confirmed chip is 16MB | n/a — this is the real fix |
| 30036653676 | `CONFIG_CONSOLE`/`CONFIG_LOG` enabled | fail (expected); boot log silent after ~1.16s | `3cdfa541` |
| 30043983799 | SWD PC-sweep at t=+2s/+10s/+20s | fail (expected); cm0 frozen at `fs_open+2` throughout | `59b8178c` |
| 30051402310 | SWD forensics: SCB fault regs + QMI/XIP state (`hang_forensics.tcl`) | fail (expected); no fault/lockup, XIP healthy, `ISRPENDING`=1 — see forensics section | pending |
| 30052303346 | SWD forensics v2: capture-wrapped reg/stack + PRIMASK/BASEPRI | fail (expected); `PRIMASK`=1, off-boundary PC, shallow garbage stack | pending |

The `hmac-to-storage` branch currently carries the flash-size fix (kept) plus
the **read-only SWD forensics diagnostic** (`scripts/diag/hang_forensics.tcl`
and a `Hang Forensics Diagnostic` step in `integration-uart`) — this one is
still active (not reverted) because the GDB-backtrace follow-up (v3) builds on
it. It must be reverted before merge, same as the earlier diagnostics. The
prior console/PC-sweep diagnostics remain cleanly reverted.

## SWD register/QMI/stack forensics (runs 30051402310 + 30052303346, 2026-07-24)

Read-only SWD capture (`scripts/diag/hang_forensics.tcl`): reset, free-run 8 s
so boot reaches the hang, halt cm0 once, and read the core registers, the
Cortex-M fault status block, the QMI/XIP peripheral state, and an SRAM stack
window. Deliberately reads **no** `0x10xx_xxxx` (XIP flash) address over SWD —
if the flash interface were wedged, such a read could stall the adapter. Two
runs: v1 got the fault/QMI reads; v2 fixed a capture bug (bare `reg`/`mdw`
output does not reach CI stdout in batch mode — only `echo`/`capture` does) and
added the register/stack dump plus the interrupt-mask registers.

**Two hardware hypotheses ruled out (confirmed on both runs):**

- **Not a CPU fault or lockup.** `CFSR = HFSR = DFSR = 0`; `DHCSR = 0x00130003`
  → `S_HALT=1` but `S_LOCKUP=0` and `S_SLEEP=0` (not locked up, not in WFI).
- **Not a wedged flash/XIP interface** — this is the scenario in which a
  littlefs→`flash_area_*`/NVS rewrite would have been *wasted*, because NVS
  hits the same `flash_range_erase`. Ruled out: `XIP_CTRL=0x00000083` (XIP
  enabled), `QMI_M0_RCMD=0x000000eb` (quad-read `0xEB` command intact),
  `QMI_DIRECT_CSR=0x00c10800` (EN=0, BUSY=0 — not stuck in a direct/serial
  transaction). The flash controller is idle and healthy.

**Also ruled out:** SMP/second-core bringup. `CONFIG_MP_MAX_NUM_CPUS=1` — this
is a single-core build, so the `idle.c` SMP-without-IPI spin path is not
compiled and cm1 sitting in the bootrom (per the PC sweep) is expected and
irrelevant.

**What the state actually is (v2, run 30052303346):**

| register | value | reading |
|----------|-------|---------|
| `PRIMASK`  | `0x01` | IRQs masked by `cpsid i` — **not** Zephyr's normal `irq_lock` (which uses `BASEPRI`, here `0x00`) |
| `ICSR`     | `0x00400000` | `ISRPENDING=1`: an IRQ is pending and **starved** — this is what drops the USB CDC link |
| `control`  | `0x02` | privileged thread on PSP |
| `pc`       | `0x101864b8` | **not an instruction boundary** — `fs_open` opens with a 4-byte `stmdb` at `0x101864b6`; `…b8` is the second halfword, *inside* it |
| `lr` / `r5`| `0x1010fb69` (`idle+0x21`) / `0x1010fb49` (`&idle`) | but `idle()` never calls `fs_open` — incoherent as a live frame |
| `r1`       | `0x0` | if this were `fs_open`'s `file_name` arg it is NULL, which returns `-EINVAL` immediately (`fs.c:145`) — can't hang *inside* `fs_open` |
| stack > SP | 2 words (`k_is_pre_kernel` ×2) then uninitialized garbage | shallow, not a coherent call chain |

The earlier "`fs_open+2` = its first real instruction" reading (PC-sweep
section) was **wrong**: `0x101864b8` is mid-`stmdb`, not an instruction
boundary. Taken together — IRQs masked via `PRIMASK`, an off-boundary PC, an
incoherent LR, a shallow garbage stack, and no CPU fault — this is **not** a
clean mutex/lock block. It reads as execution gone off the rails: a
wild/corrupted PC, or a software panic-spin (`arch_system_halt()` also does
`cpsid`+infinite-loop with no CPU fault set), reached right when the keystore
feature first touches the filesystem.

**Net for the fix decision:** the failure is a *firmware control-flow* bug, not
a flash/XIP hardware wedge — so this is not the case where moving off littlefs
is provably futile. But it is also not the clean fs-lock the rework was pitched
against, so the rework is not yet established as *the* fix either. The exact
software cause (wild jump vs. a tripped `__ASSERT`/`SPIN_VALIDATE` panic —
`CONFIG_ASSERT=y`, `CONFIG_SPIN_VALIDATE=y` are both set — vs. a lock-spin)
needs one more probe.

**Next diagnostic (v3, in progress):** attach `arm-zephyr-eabi-gdb` to the
OpenOCD gdb server (port 3333, already opened by the same step) for a real
DWARF backtrace + `info threads` (`_current` thread) + a few single-steps to
see whether the PC advances and whether a `z_fatal_error`/`arch_system_halt`
frame is present. That distinguishes panic vs. wild-jump vs. lock-spin
directly and decides the fix.

## Original next steps (superseded above, kept for the 4 MB fallback)

Everything below was written before hardware confirmation; kept for
reference/fallback only — the chip is now confirmed 16 MB, so the fallback
branch does not apply:

**Robustness follow-ups (independent of the size fix, evaluate after Step 0):**

1. Consider replacing littlefs for this data with raw `flash_area_*` or the NVS
   backend — it is a fixed-size key store + a 4-byte counter; a filesystem is
   overkill and littlefs rewrites a whole file (extra erases) on every accepted
   frame. NVS/raw erase far less often.
2. Rate-limit `loadKeyStore()` so it is not a fresh `fs_open` on every
   unrecognized-SPI frame (read-only and harmless to USB, but wasteful).
3. If per-frame sequence-number persistence proves to be a real flash-wear or
   timing problem after Step 0, either throttle `writeSequenceNumber` (persist
   every N frames / on a timer, bounded replay window) or move only the seq
   counter to a byte-writable, no-erase medium (RV3028 RTC battery-backed
   user RAM/EEPROM on i2c1 — verify ≥4 usable bytes — or FRAM/MRAM if present).

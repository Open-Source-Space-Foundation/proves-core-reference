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

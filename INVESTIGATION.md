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

**This is not yet confirmed on hardware.** If the physical chip is actually
4 MB, re-running `integration-uart`/`integration-radio` should surface a
different failure mode than before (a hardware-level flash access fault or
hang when littlefs actually reaches into out-of-range silicon, rather than the
old software-level `-EINVAL`-before-mount short-circuit) — that would be the
signal to fall back to the original plan: carve `keystore_partition` from
within 0–4 MB (e.g. shrink `slot2/test`) and fix `storage_partition` too, since
it's also out of bounds.

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

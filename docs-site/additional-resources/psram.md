# PSRAM (APS1604M) on the v5d Flight Control Board

The v5d board carries an APMemory APS1604M-3SQR 16 Mbit (2 MB) QSPI PSRAM on the
RP2350's QMI chip-select 1. The in-tree driver
(`drivers/memc/memc_rp2350_psram.c`) initializes it once at boot; afterward it
is ordinary memory-mapped RAM. Issue #285 tracks the feature.

## Memory map

| Region | Address | Notes |
|---|---|---|
| PSRAM, cached | `0x11000000` – `0x111FFFFF` | Normal access path (XIP cache) |
| PSRAM, uncached alias | `0x15000000` – `0x151FFFFF` | `XIP_NOCACHE_NOALLOC`; used by memtests |
| QMI M1 registers | `0x400D0020` – `0x400D0030` | timing / rfmt / rcmd / wfmt / wcmd |

Enabled only on `proves_flight_control_board_v5d`; the `psram0` devicetree node
is `disabled` in the shared v5 dtsi (v5/v5c/v5e opt in after their own HWIL
runs). Driver init failure is non-fatal by design — a dead or absent chip must
never brick boot.

## Using it

- **Heap (preferred):** the region is a `shared_multi_heap` pool. Allocate with
  `shared_multi_heap_alloc(SMH_REG_ATTR_EXTERNAL, size)`; nothing is placed in
  PSRAM unless a caller opts in. Check `psram_heap_ok()` first.
- **Raw:** `psram_base()` / `psram_size()` / `psram_is_ready()` from
  `drivers/memc/memc_rp2350_psram.h`.
- **F´:** `psramMonitor` (`PsramMonitor` component) telemeters `PsramSize` and
  `PsramStatus` at 1 Hz and offers `PSRAM_SELF_TEST(start_offset, length)` for
  a bounded in-flight memtest.

## Hard rules (learned on hardware — do not relearn them)

1. **QMI direct mode stalls all XIP access, including instruction fetch from
   flash.** Every instruction executed while `DIRECT_CSR.EN` is set must live
   in SRAM (`__ramfunc`) with interrupts locked. No logging, no string
   literals, no calls into flash-resident code inside the window. Violating
   this was the root cause of the `pico-ram` branch reboots.
2. **The DIRECT_RX FIFO is only a few entries deep and a full FIFO stalls the
   serial frame** — `BUSY` never clears and the CPU spins forever (silently:
   at POST_KERNEL no watchdog is armed yet). Drain RX after every transfer;
   never batch transfers and drain later.
3. **tCEM refresh bound:** CE# may stay low at most 8 µs. `MAX_SELECT` is
   derived from sys_clk with floor rounding (18 × 64 cycles at 150 MHz).
   Getting this wrong corrupts data silently, and only at temperature — if
   sys_clk ever changes, re-verify the margin on hardware.
4. **Never touch ATRANS at runtime** (XIP-cache aliasing hazard, RP2350
   datasheet §12.14.4.2).
5. **The XIP cache is write-back for the writable M1 window.** Anything that
   must observe the physical chip (memtests) goes through the uncached alias.
6. **DMA is not cache-coherent with the XIP cache.** No DMA users exist today;
   any future DMA from PSRAM must use the uncached alias or explicit cache
   maintenance.

Flash erase/program coexistence is handled by the pinned `hal_rpi_pico`:
`flash_rp2350_save_qmi_cs1()` / `flash_rp2350_restore_qmi_cs1()` preserve the
M1 registers around every `flash_range_erase/program` (verified on the bench:
M1 registers and PSRAM contents intact across FSW filesystem writes).

## Boot self-test

`CONFIG_MEMC_RP2350_PSRAM_SELF_TEST` (default `y`) runs an address-in-address
plus inverted-pattern test over the full 2 MB through the uncached alias at
every boot (~1 s). A failure marks the PSRAM not-ready and boot continues.

# Patches Directory

This directory contains patches that are automatically applied to git submodules during the build process.

## zephyr-i2c-dw-tx-empty-storm.patch

Stops a DesignWare I2C read from leaving TX_EMPTY unmasked while it waits for
RX data (`drivers/i2c/i2c_dw.c` in `lib/zephyr-workspace/zephyr`). When
`i2c_dw_data_ask()` cannot request more bytes, it now masks TX_EMPTY instead of
returning with it enabled, and the RX_FULL handler unmasks it again while bytes
are left to request.

**Why:** The driver sizes its FIFOs from `IC_COMP_PARAM_1`, which reads 0 on the
RP2350, so it assumes a FIFO depth of 1. After asking for one byte, `rx_empty`
is 0 and `i2c_dw_data_ask()` returns with TX_EMPTY still enabled. TX_EMPTY is
level triggered, so if that byte never arrives the ISR re-enters forever. Every
thread starves, including the transfer's own 100 ms timeout, so the 1 Hz rate
group, health pings and telemetry all stop. On an EM unit with all faces
powered (2026-10-26), I2C0 was idle with SDA and SCL high, `IC_INTR_MASK`
0x25f, `IC_RAW_INTR_STAT` 0x10, both FIFOs empty, and core 0 stuck in IRQ 36.
The driver state was a 2-byte INA219 read with `rx_pending` 1,
`request_bytes` 1 and `xfr_len` 2. With the patch, the transfer times out with
`-ETIMEDOUT` and the system keeps running.

**Status:** Upstream candidate for zephyrproject-rtos/zephyr. Delete this patch
once the pinned Zephyr includes an upstream fix.

**Application:** Applied by `make submodules`, like every `zephyr-*.patch` here.

## zephyr-veml6031-data-ready-timeout.patch

This patch bounds the `while (1)` data-ready poll in `veml6031_perform_single_measurement` (`drivers/sensor/vishay/veml6031/veml6031.c` in `lib/zephyr-workspace/zephyr`). After the integration-time sleep, the driver polls for at most one more integration time plus 10 ms, then returns `-ETIMEDOUT`. It also returns early if the read that clears the data-ready flag fails.

**Why:** The 1 Hz rate group reads every face's VEML6031 through ADCS and Veml6031Manager. If the data-ready flag never sets (the sensor was reset to shutdown, or another caller consumed the clear-on-read flag), the rate group spins forever. The watchdog kick and the health ping stop, and the board reboots (#538). With the patch, the worst case is 2 × integration time + 10 ms (60 ms at the default 25 ms), plus a `SensorSampleFetchFailed` event.

**Status:** Upstream candidate. Upstream Zephyr main still has the unbounded loop. Delete this patch and its `make submodules` step once the pinned Zephyr has the fix.

**Application:** `make submodules` applies this patch, with a reverse-apply check so that a second run does not fail. After it runs, `git status` shows `lib/zephyr-workspace/zephyr` as modified. This is expected. Do **not** commit the submodule pointer.

## fprime-gds-version.patch

This patch updates the `fprime-gds` version requirement in `lib/fprime/requirements.txt` from 4.1.0 to 4.1.1a2.

**Why:** The project requires fprime-gds 4.1.1a2 for specific features:
- file-uplink-cooldown argument
- file-uplink-chunk-size argument

The patch is automatically applied by the `make submodules` target to ensure version consistency and eliminate the version mismatch warning.

**Application:** This patch is applied automatically when running `make submodules` (or `make` which includes that target).

**Note:** After applying this patch, `git status` will show `lib/fprime` as modified. This is expected and should **not** be committed. The patched state is reapplied automatically on each `make submodules` run.

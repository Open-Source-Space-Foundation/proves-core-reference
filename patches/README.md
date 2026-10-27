# Patches Directory

This directory contains patches that are automatically applied to git submodules during the build process.

## zephyr-veml6031-data-ready-timeout.patch

This patch bounds the `while (1)` data-ready poll in `veml6031_perform_single_measurement` (`drivers/sensor/vishay/veml6031/veml6031.c` in `lib/zephyr-workspace/zephyr`). After the integration-time sleep, the driver polls for at most one more integration time plus 10 ms, then returns `-ETIMEDOUT`. It also returns early if the read that clears the data-ready flag fails.

**Why:** The 1 Hz rate group reads every face's VEML6031 through ADCS and Veml6031Manager. If the data-ready flag never sets (the sensor was reset to shutdown, or another caller consumed the clear-on-read flag), the rate group spins forever. The watchdog kick and the health ping stop, and the board reboots (#538). With the patch, the worst case is 2 × integration time + 10 ms (60 ms at the default 25 ms), plus a `SensorSampleFetchFailed` event.

**Status:** Upstream candidate. Upstream Zephyr main still has the unbounded loop. Delete this patch and its `make submodules` step once the pinned Zephyr has the fix.

**Application:** `make submodules` applies every `patches/zephyr-*.patch`, with a reverse-apply check so that a second run does not fail. After it runs, `git status` shows `lib/zephyr-workspace/zephyr` as modified. This is expected. Do **not** commit the submodule pointer.

## zephyr-i2c-dw-timeout-recover-bus.patch

This patch makes `i2c_dw_transfer` (`drivers/i2c/i2c_dw.c`) call the registered recovery callback after a transfer times out, while it still holds `bus_sem`.

**Why:** On EM Orange Case 001 a face power-on glitch left the DesignWare controller with bytes in its TX FIFO, the master idle and both lines high. Every later transfer timed out at 100 ms, the 1 Hz rate group slipped, and health pings went late. The driver aborts a timed-out transfer but does not reset the controller. `Drv::I2cBusMonitor` registers the callback. It aborts and disables the controller, clocks out the bus, resets the mux and enables the controller again.

**Status:** Upstream candidate. Delete it once the pinned Zephyr has an equivalent.

**Application:** `make submodules` applies every `patches/zephyr-*.patch` with the same reverse-apply check.

## fprime-gds-version.patch

This patch updates the `fprime-gds` version requirement in `lib/fprime/requirements.txt` from 4.1.0 to 4.1.1a2.

**Why:** The project requires fprime-gds 4.1.1a2 for specific features:
- file-uplink-cooldown argument
- file-uplink-chunk-size argument

The patch is automatically applied by the `make submodules` target to ensure version consistency and eliminate the version mismatch warning.

**Application:** This patch is applied automatically when running `make submodules` (or `make` which includes that target).

**Note:** After applying this patch, `git status` will show `lib/fprime` as modified. This is expected and should **not** be committed. The patched state is reapplied automatically on each `make submodules` run.

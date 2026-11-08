# Patches Directory

This directory contains patches that are automatically applied to git submodules during the build process.

## zephyr-drv2605-rtp-cancel-sync.patch

Makes `haptics_stop_output()` in RTP mode wait for a running RTP write
(`drivers/haptics/ti/drv2605.c` in `lib/zephyr-workspace/zephyr`). Upstream calls
`k_work_cancel()`, which drops a queued write but returns while a running one is
still on the bus. The patch calls `k_work_cancel_sync()` instead.

**Why:** In RTP mode the driver writes the drive level from the system workqueue,
outside `Drv2605Manager`'s lock. `Drv2605Manager` stops output when its face's load
switch turns OFF. Without the wait, that write can still be in progress when the
load switch cuts face power, which can leave I2C0 SDA stuck low (#540).

**Status:** Upstream candidate for zephyrproject-rtos/zephyr. Delete this patch once
the pinned Zephyr includes an upstream fix.

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

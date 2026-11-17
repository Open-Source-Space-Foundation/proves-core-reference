---
status: accepted
date: 2026-08-29
---

# UT lanes: helper extraction stays default; ztest only where real emulation exists

Roughly 17 in-repo components include Zephyr (or pico) headers directly and cannot build under a native F Prime UT toolchain. We keep **helper extraction** (Zephyr-free logic classes tested in `test/unit-tests/`, the repo's existing pattern) as the default lane for them, and add a **narrow ztest/Twister lane on `native_sim`** only for components whose Zephyr APIs have real in-tree emulation: FlashWorker (`flash_simulator` + MCUBoot slot partitions), RtcManager (`rtc_emul` incl. alarms), FsFormat/FsSpace (FATFS over ramdisk), and a LoadSwitch+ZephyrGpioDriver integration test over `gpio_emul`. Portable (port-only) components get full Component UTs via `make test-fprime-ut` with a native toolchain.

## Rejected alternatives (recorded because they will be re-suggested)

- **F Prime UTs under `FPRIME_PLATFORM=Zephyr`** — structurally broken: `ut.cmake` links `gtest_main`, whose `main()` collides with the native-simulator runner, and UT executables cannot link the Zephyr kernel (documented in the fprime-zephyr toolchain itself). Zero prior art in fprime, fprime-zephyr, or the community.
- **Per-test F Prime mini-deployments inside Twister apps** — legal (the one-deployment assert is per-CMake-configure) but each app pays ~30–40 s of F Prime configure and hand-rolled port wiring; may be piloted later on one component, not a lane.
- **Custom register-level chip emulators for the six sensors (TMP112, VEML6031, INA219, LSM6DSO, LIS2MDL, DRV2605)** — no emulator ships upstream (v4.3/v4.4); writing our own means mostly testing our own emulator while the managers' logic above the sensor API stays thin.
- **Zephyr header-shim layer for native builds** — a maintenance tax that mocks the exact driver-call code we would want verified.

## Consequences

- The ztest lane is Linux-only. On macOS, Twister filters all native platforms and **exits 0 with "0 executed"** — the `make` target must guard on Linux or assert executed-count > 0 from `twister.json`, or it will green-pass while running nothing.
- The "Z Tests" checkbox in the PR template predates this decision and was never defined (arrived as boilerplate in PR #21); it should be replaced by the real tier names.
- Vestigial Zephyr includes found during this analysis (LoadSwitch `gpio.h`, AntennaDeployer `kernel.h`, FsSpace — zero calls) can be removed, shrinking the Zephyr-bound set. StartupManager, initially suspected vestigial, turned out to genuinely need Zephyr (`kernel.h` for `k_uptime_seconds()`) and stays Zephyr-guarded.

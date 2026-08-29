# Testing Roadmap: F Prime Unit Testing + Software-in-the-Loop

Status: agreed 2026-08-29. Vocabulary: `CONTEXT.md`. Decisions: `docs/adr/0001`, `docs/adr/0002`.

## Goal

Bring hardware-free testing online in tiers, so component logic and subsystem behavior are verified pre-merge on GitHub-hosted runners, and the HWIL boards stop being the first place bugs are found.

## The tier model

| Tier | What runs | Where it runs | Injects at | CI job |
|------|-----------|---------------|------------|--------|
| Helper Test (exists) | Zephyr-free helper classes, gtest | macOS + ubuntu | function args | `unit-test` (exists) |
| Component UT (new) | one component via autocoded Tester, `fprime-util check` | macOS + ubuntu | F Prime ports | `fprime-ut` (new) |
| Ztest Lane (new) | Zephyr-calling helpers vs emulated devices | Linux only | Zephyr driver API | `ztest` (new) |
| SIL (new) | Subsystem Deployment + Behavioral Models + GDS pytest | macOS + ubuntu | F Prime ports (models) | `sil-radio` (new) |
| SITL Tier (new, phase 4) | full topology on native_sim + Reverse Drivers | Linux only | Zephyr driver API (fakes) | `sitl-smoke` (new) |
| HWIL (exists) | real boards, real RF | self-hosted | reality | `integration-*` (exists) |

Scope: components authored in this repo only. Submodule (`lib/`) testing belongs upstream; the fprime-zephyr platform-guard fix is the one upstream PR we file.

## Phase 0 — Housekeeping (hours)

- `make submodules` — restore the unintentionally rolled-back working tree (lib/fprime → v4.2.2, zephyr → v4.4.1 per HEAD gitlinks).
- Delete the four dangling `ReferenceDeployment/Top/Radio*.fppi` symlinks (targets `*_Usp.fppi` never existed; referenced by nothing).
- AGENTS.md: fix the stale "Integration tests are NOT run in CI" claim; document the tier model.
- PR template: replace the undefined "Z Tests" checkbox with the real tiers (Helper / Component UT / Ztest / SIL / HWIL).

## Phase 1 — Component UT infrastructure (~1–2 weeks)

The native-toolchain plumbing everything else reuses.

1. **Native build config.** New `native/` build entry (own `settings.ini`: same framework path, `library_locations` = fprime-extras only, default toolchain omitted so Linux/Darwin auto-detect). Root `CMakeLists.txt`: guard `find_package(Zephyr)` + `zephyr_include_directories()` on the Zephyr platform; delete the dead `add_subdirectory(tests)` BUILD_TESTING branch; stop force-caching UT flags off for native builds.
2. **Zephyr-bound component guard.** In `Components/CMakeLists.txt` (and `Components/Drv/`), wrap Zephyr-header-bound components in `if(FPRIME_PLATFORM STREQUAL "Zephyr")`. Spike: shared port/type FPP used by portable consumers (e.g. `Drv.temperatureGet` consumed by ThermalManager) may need to move into the portable `Drv/Types` module so autocode resolves natively.
3. **Upstream PR** to fprime-zephyr: platform-guard its module registrations (`fprime-zephyr.cmake`), so `library_locations` need not differ per toolchain long-term.
4. **Exemplar Component UTs** (the pattern others copy): **Watchdog** (small; GPIO-port stub; regression for the unopened-GPIO lockup class) and **ModeManager** (safe-mode transitions incl. the cases HWIL permanently skips for needing manual voltage). Stretch: ProvesRouter. Uncomment `register_fprime_ut`, add `test/ut/` with autocoded Tester + gtest.
5. **Vestigial-include cleanup**: drop unused `rtc.h`/`gpio.h`/`kernel.h` includes from StartupManager/LoadSwitch/AntennaDeployer — moves them into the portable set.
6. **Helper-extraction additions**: StartupManager boot-count/persistence logic (its real logic is Os::File + uptime; high incident history).
7. **Wiring**: `make test-fprime-ut` (native generate + `fprime-util check`); CI job `fprime-ut` on ubuntu-latest (checkout, submodules, venv, no Zephyr SDK). Works locally on macOS via Darwin platform.

Policy (agreed): tests-on-new-components is an AGENTS.md convention + review culture, not a coverage gate. Retrofit deliberately by incident history: radio path, startup/boot-count, watchdog first.

## Phase 2 — Ztest Lane (~1 week, parallelizable with Phase 3)

Only the emulation-backed set (ADR 0002): FlashWorker (flash_img/mcuboot flow on `flash_simulator` with slot partitions — verifiable trailer state), RtcManager (rtc_emul alarms/callbacks), FsFormat/FsSpace (FATFS mkfs/statvfs over ramdisk — empirically answers whether the `(uintptr_t)partition_id` cast is a valid FATFS dev_id), LoadSwitch+ZephyrGpioDriver over gpio_emul.

- Layout: `PROVESFlightControllerReference/test/ztest/<name>/` 4-file Twister apps (CMakeLists, prj.conf, testcase.yaml, src). Tests target Zephyr-calling helper code — no F Prime autocode in test apps.
- `make test-ztest`: invoke Twister with `-T test/ztest -p native_sim/native/64`; **guard**: fail unless `uname == Linux` and `twister.json` executed-count > 0 (macOS silently filters to 0-executed, exit 0).
- Deps already satisfied: zephyr `scripts/requirements.txt` is in the venv (verified); no Zephyr SDK needed (host toolchain); junit output built-in (`twister_report.xml`) → upload in new `ztest` CI job.

## Phase 3 — Radio SIL (~1–2 weeks)

1. **`SilRadio` Subsystem Deployment** (posix): ComCcsdsLora subtopology (real ComQueue/FrameAccumulator/CCSDS stack/TcSecurityDeframer/ProvesRouter) + minimal command/event/telemetry core + `Svc.PosixTime` + **RadioModel** replacing `Zephyr.LoRa`.
2. **RadioModel**: implements `Svc.Com` + the `enableTransmit`/`disableTransmit`/`loraFirstStart` ports StartupManager normally drives; bridges GDS bytes over TCP; fault injection as F Prime commands + telemetry (drop N frames, corrupt CRC, delay, radio-busy, ALARM emission). Lives outside the flight component list (e.g. `Components/Sim/`), never linked into ReferenceDeployment.
3. **pytest**: `test/sil/` reusing `IntegrationTestAPI`, the auth framing plugin, and `common.py` retry helpers; fault schedules scripted via RadioModel commands. Regression targets from incident history: missing 4-byte LoRa header handling, ALARM-driven reboot behavior, CONTINUOUS_WAVE repeat calls, sequence-number desync.
4. **Wiring**: `make test-sil-radio` (build native deployment, launch GDS with TCP adapter + SIL dictionary, pytest); CI job `sil-radio` on ubuntu-latest, parallel to `build`. Runs locally on macOS.
5. **Follow-on**: `SilStartup` deployment (StartupManager portable after Phase 1 cleanup + FileHandling) for boot-count/persistence scenarios — same harness, second subsystem per agreed priority.

## Phase 4 — SITL Tier (stretch, ~2–3 weeks)

Motivated by incident history: the rate-group lockup (unopened gpioWatchdog) and boot-hang (thread-pool sizing) classes are whole-topology bugs invisible to every tier above.

1. **Reverse Drivers**: one generic scriptable `sensor_driver_api` fake serves all sensor managers (devices are injected via `configure()`, not devicetree-looked-up); gpio/rtc/flash come from in-tree emuls. Exclusions: Drv2605Manager (vendor-private API), BootloaderTrigger (pico bootrom) — HWIL-only forever.
2. **native_sim overlay + prj.conf variant**: recreate the ~45 devicetree node labels; extra pty UART for `cdc_acm_uart0` so the existing GDS uart adapter attaches; `Update.StubWorker` swap (one line in `project/config/UpdateConfig.fpp`).
3. **Spikes before committing**: DYNAMIC_THREAD pool (25×4KB) on posix arch; rate-group timing under `-rt`; file-backed sim-flash persistence across process relaunch (would enable boot-count/reset cycles DoL-style — `sys_reboot` = process exit).
4. **CI**: `sitl-smoke` job — boot to topology-up + command round-trip + clean rate-group ticks. Closed-loop scripted-physics tests grow from there.

## CI end state

PR-blocking on ubuntu-latest: `lint`, `unit-test`, `fprime-ut`, `ztest`, `sil-radio` (later `sil-startup`, `sitl-smoke`). HWIL (`build` → `integration-uart`/`integration-radio`) unchanged on self-hosted. While touching ci.yaml: remove the vestigial `cache-hit` guards referencing nonexistent step ids.

## Risks / open spikes

- FPP port/type entanglement between portable consumers and Zephyr-bound manager modules (Phase 1.2) — biggest unknown in the native build.
- ComCfg / zephyr-config config-module behavior under a native toolchain without fprime-zephyr in `library_locations`.
- fprime-gds TCP adapter + auth framing plugin against a posix deployment (expected clean — ComStub path is transport-agnostic — but unproven here).
- Twister/macOS silent-pass footgun (mitigated by guard in Phase 2).
- Upstream fprime-zephyr PR timeline; the local `native/` settings.ini split removes the dependency until it lands.

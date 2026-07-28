# HMAC key → internal-flash storage — progress tracker

Plan source: `~/.claude/plans/quirky-forging-possum.md`
Branch: `hmac-to-storage`

Status legend: [ ] todo, [~] in progress, [x] done

## 1. Flash partition + littlefs mount
- [x] Add `keystore_partition` to `proves_flight_control_board_v5.dtsi` `&flash0`, shrink `storage_partition`
      (keystore_partition@0x400000 256KB; storage_partition@0x440000 0xBC0000)
- [x] Add `zephyr,fstab,littlefs` node mounted at `/keys` (lfs1, automount)
- [x] `prj.conf`: `CONFIG_FILE_SYSTEM_LITTLEFS=y` (correct Kconfig name, not FS_LITTLEFS)
- [x] Automount confirmed via Zephyr Kconfig.littlefs: FS_LITTLEFS_FSTAB_AUTOMOUNT defaults y when DT
      node has `automount` + CONFIG_FLASH_MAP=y (already on for all 3 board defconfigs). No explicit
      fs_mount() needed in Main.cpp.

## 2. Key store type + persistence (TcSecurityDeframer)
- [x] Add FPP `AuthKeySlot` / `AuthKeyStore` (2 slots) type + `KeyStoreProvisionStatus` enum, in `TcSecurityDeframer.fpp`
      (verified accessor names `getvalid/getspi/getkey/setvalid/setspi/setkey`, `AuthKeyStore::SIZE`, `operator[](U32)`
      by running `fpp-to-cpp` on a scratch copy of the struct/array — see scratch notes below)
- [x] `Authenticator.cpp/.hpp`: added `importHmacKeyBytes` (raw bytes, used by key-store reimport) with
      `importHmacKey` (hex) now a thin wrapper; added `destroyHmacKey`; exposed `parseHexKey`;
      `authenticatePacket` now takes `keyId` by value (was `uint32_t&`, never mutated)
- [x] `TcSecurityDeframer.hpp/.cpp`: `KEY_STORE_FILE_PATH` param, `loadKeyStore()`/`writeKeyStore()`/`importKeyStore()`/
      `findKeyIdForSpi()`/`activeKeyCount()` helpers, second `m_keyStoreLock` mutex (separate from seq-num lock;
      dataIn_handler takes keyStoreLock then seqLock, only ordering with both — no deadlock risk).
      configure() loads store, no FW_ASSERT on missing/empty (keyless boot supported)
- [x] `dataIn_handler`: `validatePacket` now takes the key store; on `SpiInvalid` reloads store from disk once
      and retries before giving up
- [x] `PROVISION_KEY` / `ADD_KEY` / `REMOVE_KEY` command handlers + KeyProvisioned/Failed, KeyAdded/Failed,
      KeyRemoved/Failed, KeyStoreReadFailed/WriteFailed events, ActiveKeyCount telemetry
- [x] `Validator.cpp`: `spiValid` checks against active slots instead of `spi == 0` (takes `const AuthKeyStore&`)
- [x] `TcSecurityDeframer.fpp`: `SEQ_NUM_FILE_PATH` default -> `/keys/sequence_number.bin`
- [x] Removed `#include "AuthDefaultKey.h"` / `AUTH_DEFAULT_KEY` usage from `TcSecurityDeframer.cpp`;
      deleted generated `AuthDefaultKey.h` from TcSecurityDeframer dir (it's gitignored, not tracked)

## 3. Router bootstrap allowlist
- [x] `Bypasser.cpp`: added `0x2100B002`/`0x2200B002`/`0x2300B002` (UART/LoRa/Sband PROVISION_KEY) to
      `kBypassOpCodes`. Derivation verified against the real (pre-existing, stale) dictionary at
      `build-artifacts/zephyr/fprime-zephyr-deployment/dict/ReferenceDeploymentTopologyDictionary.json`:
      opcodes are `instance_base + local_index`, where local_index is 0-based over *user commands only*
      (GET_SEQ_NUM=0, SET_SEQ_NUM=1) followed by PRM_SET/PRM_SAVE pairs per param in declaration order.
      My new commands are declared right after SET_SEQ_NUM and before any params, so
      PROVISION_KEY=2, ADD_KEY=3, REMOVE_KEY=4 (only PROVISION_KEY needs a bypass entry).
      Caveat: the stale dict only had ComCcsdsUart/ComCcsdsLora instances (no Sband), so 0x2300B002
      is derived by pattern from the pre-existing (already in file) 0x2300B000 Sband entry, not directly
      confirmed. **Re-verify opcodes with `make build` + fresh dict.json before flight/CI trust.**

## 4. Ground plugin
- [x] `Framing/src/authenticate_plugin.py`: `get_default_auth_key_from_header` -> `get_auth_key_from_env`,
      reads `PROVES_AUTH_KEY` env var, raises ValueError with clear message if neither CLI arg nor env set
- [x] `tools/yamcs/proves_adapter.py` also imported the removed function — updated to `get_auth_key_from_env`
      (not called out in plan explicitly but same removal would have broken this importer)

## 5. Build / Makefile / provisioning
- [x] `Makefile`: dropped `generate-auth-key` target + from `generate` deps, `AUTH_DEFAULT_KEY_HEADER`/
      `AUTH_KEY_TEMPLATE` vars, `AuthDefaultKey.h` copy line in `copy-secrets`
- [x] Deleted `scripts/generate_auth_key_header.py`, `scripts/generate_auth_default_key.h`
- [x] Added `PROVESFlightControllerReference/test/int/provision_key_test.py` (marker `provision_key`),
      registered marker in `pytest.ini`, added `and not provision_key` to Makefile default `FILTER`.
      Idempotent: PROVISION_KEY fails with `NotEmpty` if the board was already provisioned by a
      prior CI run (key store lives on internal flash, survives reflashing) — the test treats that
      as success rather than failure, since the store already holds the CI secret key.

## 6. CI
- [x] Removed all 3 "Set Authentication Key" steps (build, integration-uart, integration-radio) that
      wrote `AuthDefaultKey.h` in `.github/workflows/ci.yaml`.
- [x] No job-wide `PROVES_AUTH_KEY` export. Instead, each step that either starts a GDS process
      (`make gds-integration`, whose `AuthenticateFramer` plugin reads the env var at construction
      and raises if unset) or itself needs the key value (`provision_key_test.py`, which reads
      `os.environ["PROVES_AUTH_KEY"]` directly to build the command arg) gets its own `env:` block
      with `PROVES_AUTH_KEY: ${{ secrets.AUTH_KEY }}`. Steps that only talk to an already-running
      GDS over the network (Sync Sequence Number, Format Filesystem, Run UART/Radio Integration
      Tests) don't need it. 6 steps scoped this way total (3 in integration-uart: two "Start GDS" +
      "Provision Key"; 3 in integration-radio: "Bootstrap Sequence Number over UART", "Sync Sequence
      Number over UART", "Start GDS on LoRa Passthrough"). The build job needs no key at all now
      (nothing to bake into the image). No explicit `--authentication-key` CLI flag needed since the
      plugin reads the env var by default.
- [x] Added a "Provision Key" step (`make test-integration FILTER=provision_key`) right after the
      first `Start GDS` in integration-uart, and inside the "Bootstrap Sequence Number over UART"
      block in integration-radio (before the LoRa sync, since key-store propagation means
      provisioning once over UART covers the LoRa instance too).
      Added `and not provision_key` to the radio job's main test-run FILTER.

## 7. Docs
- [x] Update `AGENTS.md` "Authentication & Security" section
- [x] Update `PROVESFlightControllerReference/Components/TcSecurityDeframer/docs/sdd.md`

## Build verification
- `make generate build` (not `uv run ...` directly — Makefile targets resolve the right Python env)
  ran a real build and caught real errors, now fixed:
  - FPP-generated accessor names are `get_valid/get_spi/get_key/set_valid/set_spi/set_key` (with
    underscores), not `getvalid/getspi/...` as guessed earlier. Fixed all call sites in
    `TcSecurityDeframer.cpp` and `Validator.cpp` via sed.
  - New `ActiveKeyCount` telemetry channel (per deframer instance) was never referenced in any
    telemetry packet, which `fpp-to-dict` treats as a hard error ("neither used nor marked as
    omitted"). Added `ComCcsdsLora.tcSecurityDeframer.ActiveKeyCount` /
    `ComCcsdsUart.tcSecurityDeframer.ActiveKeyCount` to the `Security` packet in
    `ReferenceDeploymentPackets.fppi` (Sband entry commented out, matching the existing
    `CurrentSequenceNumber` pattern in that same packet — Sband instance not present in this build).
  - Re-ran `make generate build` after these fixes: **clean full build**, FLASH 69.81%, RAM 62.32%,
    `zephyr.uf2`/`bootable.uf2`/dictionary/XTCE all generated successfully.

## Verification
- [x] Unit tests (`make test-unit`, pure C++ only, no F Prime/Zephyr): `parseHexKey` (direct + wrapped),
      `importHmacKeyBytes`/`destroyHmacKey`, and Validator SPI/sequence-number rules. Note:
      Validator.cpp had started depending on the FPP-generated `AuthKeyStore` (`Fw::Serializable`),
      which broke this test target's no-F-Prime contract — decoupled it via a new plain
      `ActiveSpiSlots`/`ActiveSpiSlot` type in `Types.hpp`; `TcSecurityDeframer::activeSpiSlots()`
      projects `m_keyStore` into it before calling `validatePacket`. Store-mutation rules
      (provision-only-when-empty, add fails at 2, remove fails at 1) live in the F-Prime-dependent
      command handlers, not pure functions, so they're out of scope for this gtest target — the
      commented-out `register_fprime_ut` block remains for a future on-target/component test pass.
- [x] `make build` with no `AuthDefaultKey.h` — clean build confirmed (see above)
- [x] `make check-console-disabled` — OK, Zephyr console disabled
- [ ] CI green — **still blocked, but root cause narrowed**. Pushed the 16MB flash
      fix (below) and re-ran CI (run 30034861047): `Flash Firmware` step's OpenOCD
      output confirms the chip really is 16MB (`w25q128fv/jv ... size = 16384 KiB`),
      so the fix itself is correct and necessary. But `integration-uart`/
      `integration-radio` **still fail with the identical symptom** — GDS
      `comm.py.log` shows repeated `device disconnected` serial exceptions starting
      ~19s after boot, `CMD_NO_OP` never gets a response. So the flash-size
      mis-declaration was real but **not the sole cause**. Revised theory: now that
      `/keys` can actually mount, littlefs's first-ever format on this partition is
      the first code path in this branch that reaches `flash_rpi_write`/
      `flash_rpi_erase`, both of which hold `irq_lock()` for the entire
      erase/program call with no yielding — i.e. the original `PROBLEM.md`
      interrupt-stall theory may be correct after all, it just couldn't fire before
      (every op was rejected by `-EINVAL` pre-`irq_lock`). (That theory was later disproved; see commit `ec0bdb37`.) **Next: a temporary `CONFIG_LOG=y` diagnostic CI
      run** (same pattern as the prior fault-register diagnostic commits) to see
      whether the stall is the one-time format or ongoing per-frame reloads.

## CI blocker — ROOT-CAUSED AND FIXED (2026-07-27), see commit ec0bdb37

- [x] **Root cause: CommandDispatcher opcode-table overflow.**
      `project/config/CommandDispatcherImplCfg.hpp` had
      `CMD_DISPATCHER_DISPATCH_TABLE_SIZE = 350` against a deployment already at
      **348** commands. This branch adds PROVISION_KEY/ADD_KEY/REMOVE_KEY to
      `TcSecurityDeframer`, and there are **two** instances (ComCcsdsUart,
      ComCcsdsLora) -> 6 new commands, **354 > 350**.
      `CommandDispatcherImpl.cpp:35` `FW_ASSERT`s when the RedBlackTreeMap
      insert fails, so the 351st registration **panics the board during boot**
      (`z_fatal_error(reason=4)` via `z_arm_svc` -- a `k_panic`, not a CPU
      fault, which is why every fault-vector probe found nothing). Downlink
      never starts -> GDS `device disconnected` -> CI fails.
      **Fix: raised to 512.**
      Confirmed on the local bench with a single-variable A/B, both full clean
      `make generate build`s, measuring F' telemetry bytes off the board CDC in
      a 25s window: `main` 1328 bytes @t+1.0s; branch as-shipped **0**; branch +
      table 512 **1392 bytes @t+1.01s**; branch + table back to 350 **0**.
      Final verified build: **2200 bytes in 30s @t+1.01s**.

- [x] **Second, independent defect: littlefs was never compiled in.**
      `west.yml`'s `name-allowlist` imported `fatfs` but not `littlefs`, so the
      module never reached `zephyr_modules.txt`, `ZEPHYR_LITTLEFS_MODULE` was
      undefined, and Kconfig **silently dropped** `CONFIG_FILE_SYSTEM_LITTLEFS=y`
      ("LittleFS module not available"). No `lfs_*` symbols in the image, `/keys`
      never existed, every `fs_open("/keys/...")` failed -- the entire key-store
      feature was inert while the build stayed clean.
      **Fix: added `littlefs` to the allowlist and pinned it as an explicit
      project** (like every other module) so it lands under `lib/zephyr-workspace/`
      rather than the workspace topdir. Verified `CONFIG_FILE_SYSTEM_LITTLEFS=y`,
      `CONFIG_FS_LITTLEFS_FSTAB_AUTOMOUNT=y`, 20 `lfs_*` symbols in the ELF.
      Not the CI blocker, but the feature cannot work without it.

- [x] **Feature verified end to end on hardware (2026-07-27).** Board on
      `/dev/tty.usbmodem1101`, GDS on the USB CDC, state read back over SWD:
      1. **`/keys` mounts and formats on internal flash.** littlefs superblock
         magic present at `0x10400008` with block_size 0x1000 and block_count
         0x40 -- exactly the 256 KB `keystore_partition` geometry.
      2. **PROVISION_KEY works on a keyless board** over the unauthenticated
         (bypass-allowlisted) link: `m_keyStore.elements[0]` went to
         `valid=1 spi=0` with the provisioned key bytes.
      3. **The key survives a cold reboot.** After `reset` (RAM re-zeroed by
         `arch_bss_zero`), slot 0 is `valid=1` with the same bytes -- i.e.
         `configure()` -> `loadKeyStore()` read it back off flash.
      4. **The flash-stored key authenticates uplink.** `SET_SEQ_NUM 12345`
         requires authentication (not bypass-allowlisted) and took effect:
         `m_sequenceNumber == 12345`.
      5. **The sequence number survives a cold reboot**: still 12345 after
         reset, read back from `/keys/sequence_number.bin`, key still valid.
      6. **Erasing the partition re-formats cleanly.** After erasing
         `0x10400000+0x40000` over SWD the board came back keyless
         (`valid=0`, `seqnum=0`) with a fresh littlefs superblock.
      7. **Bypass path confirmed:** on a keyless board the router shows
         `routed=3 bypassed=3 rejected=0` -- allowlisted opcodes are dispatched
         without a key, which is what makes bootstrap possible.
      **Section 3 opcodes re-verified against the fresh dictionary:**
      `ComCcsdsUart/Lora.tcSecurityDeframer.PROVISION_KEY` are `0x2100B002` /
      `0x2200B002`, matching `Bypasser.cpp` exactly. The TODO caveat there is
      resolved. (The Sband entry `0x2300B002` is still unconfirmed -- that
      instance is not built.)

## Current goal: integration tests green on the local bench AND in CI

Tracked in `INVESTIGATION.md`. The firmware blockers are fixed and verified on
hardware; what is left is the test path plus one design decision.

- [x] **`provision_key_test.py` passes on the bench (2026-07-28).** The GDS
      desync hypothesis was **wrong**. Two real causes:
      1. **Mis-built predicate.** `await_event(satisfies_any([event_pred, ...]))`
         -- `get_event_pred` only passes an argument through when it is already
         an `event_predicate`, so a `satisfies_any` was used as the *event-ID*
         predicate and its inner `EventData` checks were evaluated against an
         int. Never matched; `evt` came back `None` and blew up as
         `AttributeError`. Fixed with
         `is_a_member_of([translate_event_name(...), ...])` + an explicit
         `assert evt is not None`.
      2. **A halted board.** Earlier sessions left the target halted under SWD,
         which drops the USB CDC, so `start_gds`'s `CMD_NO_OP` failed and
         errored the whole directory in setup.
      Verified on hardware both ways: keyless board (keystore erased over SWD)
      -> `KeyProvisioned` -> `sync_sequence_number` passes, i.e. the
      flash-stored key authenticates; and already-provisioned board ->
      `NotEmpty` -> treated as success.

- [x] **`start_gds` now explains itself.** The bare `assert gds_working` is
      replaced by a message naming the command, the attempt count and the last
      exception -- it gates every test in the directory, so its failure used to
      surface as 40-odd unexplained setup errors. The redundant `start_gds`
      dependency was also dropped from `recover_from_safe_mode`; note the
      originally-planned "make it opt-in" fix would **not** have helped, because
      every test file already requests `start_gds` directly.

- [x] **Full bench suite green: 30 passed, 0 failed (2026-07-28).** Four
      unrelated bench failures diagnosed and resolved (table in
      `INVESTIGATION.md`): LOW_BATTERY auto-safe-mode cycling the face load
      switch until the face I2C bus wedged (new `--no-battery` option drops
      `SafeModeEntryVoltage` to 0 per test); a real pre-existing race in
      `rtc_test`'s `uplink_sequence_and_await_completion`, which uplinked
      without waiting for `CreateDirectory /seq` to land; `/antenna` missing
      after a format until the next boot; and two tests that genuinely need
      hardware this bench lacks (`drv2605` -> `requires_battery`, `safe_09` ->
      `requires_watchdog_jumper`). Markers are inert in CI, which never passes
      `--bare-flight-controler-board`.

- [ ] **Get CI green.** Bench is green and the remote branch is 3 commits
      behind, so CI has never run with the CommandDispatcher table fix. Removed
      the two `Hang Forensics` diagnostic steps from `ci.yaml` -- they halted the
      target over SWD immediately before GDS started, which is precisely the
      failure mode that made Issue 1 look real.

- [ ] **Decide how a mis-provisioned key is recovered.** Confirmed behaviour,
      needs an explicit call rather than a quiet patch: `PROVISION_KEY` is
      refused on a non-empty store (`NotEmpty`), `REMOVE_KEY` refuses the last
      key (`LastKey`), and `ADD_KEY` needs an already-authenticated link -- so a
      board keyed with the wrong value is unreachable from the ground. Recovery
      on the bench required an SWD erase of `keystore_partition`. Fine on the
      bench, fatal in flight. Options weighed in `INVESTIGATION.md` (accept it
      with a verified ground procedure; bypass-allowlist `ADD_KEY`; authenticated
      `CLEAR_KEY_STORE`; two-slot bootstrap provisioning; time-boxed post-boot
      bypass window) -- each trades security against recoverability.

- [ ] **Watch the other zero-headroom config constants.** Same failure mode,
      same file tree: `MAX_PACKETIZER_CHANNELS = 202` vs 191 channels in use,
      `MAX_PACKETIZER_PACKETS = 22` vs exactly 22 packets. Adding commands,
      channels or packets to this deployment requires checking these against the
      generated dictionary -- they assert at boot rather than degrading.

- [ ] **Write up the SWD/GDB diagnostic helpers as an ADR, and point the agent
      instructions at it.** The probes built while root-causing this branch are
      generally useful for any "board is silent / GDS sees nothing" failure on
      this hardware, and re-deriving them cost most of a session. Capture in a
      new ADR (there is no `docs/adr/` yet -- this would be the first):
      - `scripts/diag/hang_thread_walk.{sh,gdb}` -- reset, free-run, halt twice
        N s apart; clock/SysTick/interrupt state, a walk of `_kernel.threads`
        naming each F' task, the `timeout_list`, the whole downlink chain's
        state, and a per-thread CPU-time diff between the two halts.
      - `scripts/diag/downlink_trace.{sh,gdb}` -- breakpoints on every hop of
        the `comStub -> framer -> aggregator -> spacePacketFramer -> comQueue`
        com-status path.
      - The older `hang_forensics.tcl` / `hang_gdb.sh` / `hang_fault_bp.*`.
      Non-obvious things the ADR should record, all of which cost real time:
      - Use the **raspberrypi OpenOCD fork**, not a nix/brew build.
      - Sequencing run/halt via `monitor` leaves gdb serving **stale registers**;
        detach + reconnect to resync (`monitor gdb sync` + `stepi` can resume the
        target when the halt lands mid-ISR).
      - **Always resume the target** before detaching -- a halted board drops its
        USB CDC, which makes GDS see nothing and silently no-ops any command.
      - A swapped-out Cortex-M thread's `callee_saved.psp` points straight at the
        exception frame (LR `+0x14`, PC `+0x18`); callee regs live in the
        `k_thread`. Reading `+0x34`/`+0x38` yields F' object addresses that look
        exactly like plausible wild pointers -- this produced a multi-day red
        herring in the earlier investigation (git history, pre-`ec0bdb37`).
      - `PRIMASK=1` at `arch_cpu_idle+18` is the **normal** idle sequence, not a
        masked spin.
      - Prefer monotonic `base.usage.total` over saved psp/PC when asking "did
        this thread make progress" -- a healthy thread re-blocking at the same
        line reproduces byte-identical values.
      - **`make build` does not re-derive Kconfig from device-tree changes**; use
        `make generate build`.
      Then add a diagnostics section to the repo's agent instructions pointing at
      the ADR. **Note:** the repo has `AGENTS.md`, not `CLAUDE.md` -- decide
      whether to add the section to `AGENTS.md`, or add a `CLAUDE.md` (symlink or
      stub) so both agent toolchains pick it up.

- [ ] **Remove the diagnostics before merge:** `scripts/diag/hang_thread_walk.*`,
      `scripts/diag/downlink_trace.*`, `scripts/diag/hang_fault_bp.*`,
      `scripts/diag/hang_forensics.tcl`, `scripts/diag/hang_gdb.sh`, the
      `Hang Forensics Diagnostic` step in `ci.yaml`, and the stray capture logs.

### Build-system trap (cost several hours this session)
`make build` does **not** re-derive Kconfig from device-tree changes -- it left
`CONFIG_FLASH_SIZE=4096` while the DTS said 16 MB, putting `keystore_partition`
out of bounds so the `/keys` automount panicked on
`__ASSERT_NO_MSG(block_size != 0)` (`littlefs_fs.c:787`). Purely an artifact of
the stale config, and it invalidated several intermediate bisect results.
**Always `make generate build` after touching the device tree.**

## Superseded — original "Suggested fix" notes, kept for the audit trail

- [x] **Primary fix**: changed `&flash0 { reg = <0x10000000 DT_SIZE_M(4)>; }` →
      `DT_SIZE_M(16)` in `proves_flight_control_board_v5.dtsi` (shared by v5c/v5d/v5e).
      Confirmed correct by CI hardware run 30034861047: OpenOCD reports the real chip
      is `w25q128fv/jv ... size = 16384 KiB`. `CONFIG_FLASH_SIZE` now follows to 16384
      (was 4096), build otherwise unaffected (FLASH/RAM usage unchanged). **Necessary
      but not sufficient** — see next item.
- [ ] **Diagnose the remaining stall**: same CI run still fails identically
      (repeated GDS `device disconnected` serial exceptions, no response to
      `CMD_NO_OP`). Revised theory: `/keys` mounting for the first time means
      littlefs's format now actually reaches `flash_rpi_write`/`flash_rpi_erase`,
      which hold `irq_lock()` for the whole erase/program call — `PROBLEM.md`'s
      original interrupt-stall theory may be correct, it just had nothing to act on
      before this fix.
      Ran (and reverted) a temporary `CONFIG_LOG=y`+console diagnostic (CI run
      30036653676): boot log shows normal USB init through ~1.16s then **total
      silence** for the remaining ~39s — no more log lines, no TM-frame noise
      either. Consistent with a full hang very early in boot, but doesn't pinpoint
      where. Ruled out a stale `PICO_FLASH_SIZE_BYTES` hard_assert in the Pico
      SDK's `flash_range_erase` — that macro isn't defined in this Zephyr build.
      Diagnostic reverted (console can't coexist with a working GDS link — see
      `scripts/check_console_disabled.py`).
      **Ran the SWD PC-sweep (CI run 30043983799, reverted after): hang located.**
      cm0's pc/lr/sp/xpsr are byte-for-byte identical at t=+2s/+10s/+20s after
      reset — zero forward progress for 20+ seconds. Resolved against the
      build's symbols: `pc=0x101864b8` is `fs_open+2` (its first real
      instruction), `lr=0x1010fb69` is inside Zephyr's `idle()` (the context
      that runs early `SYS_INIT`/fstab-automount code before the scheduler
      starts other threads) — i.e. cm0 is frozen at the very first file
      operation this branch performs after `/keys` mounts (almost certainly
      `TcSecurityDeframer::configure()`'s `loadKeyStore()` opening a virgin
      key-store file for the first time). cm1 sampled `pc=0x19e` (a bootrom
      address) unchanged too — cm1 was never launched into Zephyr code at all,
      this app runs single-core. Two candidate mechanisms (both point at the
      same fix, see the earlier investigation in git history for full
      reasoning): (a) something in the mount/format path already holds
      `irq_lock()` in a flash erase/program that never returns, and `fs_open`'s
      first action (a shared fs mutex) blocks on it forever; (b) a dual-core
      interaction given cm1's unusual unlaunched state, though the vendored
      `flash_range_erase`/`flash_range_program` in this tree don't show an
      obvious multicore-lockout wait. **Next: either replace the littlefs
      `/keys` mount with raw `flash_area_*`/NVS (sidesteps this class of bug
      regardless of exact mechanism — see Robustness follow-ups below), or dig
      further into which specific call inside the mount/format/create path
      never returns.**
      **2026-07-27 — the "stall" does not exist.** Built and ran the v5
      thread-walk probe (`scripts/diag/hang_thread_walk.{sh,gdb}`) on the local
      bench. Across two halts 4s apart the board is *fully healthy*: clock
      advancing (+40040 ticks = 4.004s), no fault (`CFSR=HFSR=0`), no masked
      IRQs, the 1ms base-rate `k_timer` queued and firing, all three rate
      groups cycling, main looping in `startRateGroups()`, and 89% idle. Every
      earlier "hang" reading was a misread healthy idle CPU (`PRIMASK=1` +
      `arch_cpu_idle` is the normal `cpsid i; wfi; cpsie i`), a stale FPB
      breakpoint, or callee-saved registers misread as a PC — there is no wild
      jump, stack overflow, fs-lock deadlock or fatal-halt spin.
      **The real failure is the downlink:** `usbd_thread`,
      `udc_rpi_pico_thread_0` and `ComCcsdsUart::comQueue` consume *zero* CPU
      cycles while `ComCcsdsUart::aggregator` burns 1.4M, and the host reads 0
      bytes in 15s from the board CDC. Telemetry is produced and aggregated but
      never dequeued to the com driver, and the USB device stack is dormant.
      **Next: chase the UART/USB downlink path**, not the filesystem. See
      the earlier investigation in git history (pre-`ec0bdb37`).
- [ ] **Robustness follow-ups (evaluate once the stall is diagnosed):** (a) consider
      raw `flash_area_*`/NVS instead of littlefs for this fixed-size store (avoids
      the format-time erase burst and any long single-call erase/program under
      `irq_lock`); (b) rate-limit `loadKeyStore()` so it isn't a fresh `fs_open` per
      unrecognized-SPI frame; (c) if per-frame `writeSequenceNumber` proves a real
      wear/timing problem, throttle it or move only the seq counter to a no-erase
      medium (RV3028 RTC user RAM / FRAM).

## Notes / decisions while implementing
(append here as work progresses)

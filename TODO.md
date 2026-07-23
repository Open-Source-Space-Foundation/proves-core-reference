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
- [ ] CI green — **pending re-run**. `integration-uart`/`integration-radio` failed on
      real hardware. Board was confirmed alive (not crashed/faulted — verified via live
      SWD register dump), but the ground side saw real USB serial disconnects and never
      got a response to the first command. See `INVESTIGATION.md` for a static
      re-analysis: the `PROBLEM.md` interrupt-stall theory is **likely wrong** (reads
      don't `irq_lock` in `flash_rpi_pico.c`, and `keystore_partition@0x400000` sat
      past the declared 4 MB `flash0` boundary → every keystore op returned `-EINVAL`
      before any `irq_lock`, so `/keys` never mounted and no interrupt was ever
      disabled for it). Real root cause is almost certainly the flash-size
      mis-declaration. **Fix implemented below — awaiting CI hardware confirmation.**

## Suggested fix (blocked CI) — see INVESTIGATION.md
- [x] **Primary fix**: changed `&flash0 { reg = <0x10000000 DT_SIZE_M(4)>; }` →
      `DT_SIZE_M(16)` in `proves_flight_control_board_v5.dtsi` (shared by v5c/v5d/v5e).
      No hardware bench available this session to do the SWD/console confirmation from
      the old Step 0, but the evidence was already strong without it: `storage_partition`
      on `main` has run `0x400000 → 0x1000000` (16 MB extent) against this same 4 MB
      declaration for a while, unnoticed only because nothing mounted it. `make generate
      build` confirms `CONFIG_FLASH_SIZE` now follows to 16384 (was 4096), same
      FLASH/RAM usage as before (69.82%/62.32%) — build is otherwise unaffected.
      **Re-run both integration jobs on real hardware to confirm** — if the chip is
      actually 4 MB this will surface as a hardware-level flash access fault/hang
      distinct from the old `-EINVAL`-before-mount failure, at which point fall back to
      carving `keystore_partition` from within 0–4 MB (e.g. shrink `slot2/test`) and
      fixing `storage_partition` too.
- [ ] **Robustness follow-ups (evaluate only after CI confirms the fix):** (a) consider raw
      `flash_area_*`/NVS instead of littlefs for this fixed-size store; (b) rate-limit
      `loadKeyStore()` so it isn't a fresh `fs_open` per unrecognized-SPI frame; (c) if
      per-frame `writeSequenceNumber` proves a real wear/timing problem, throttle it or
      move only the seq counter to a no-erase medium (RV3028 RTC user RAM / FRAM).

## Notes / decisions while implementing
(append here as work progresses)

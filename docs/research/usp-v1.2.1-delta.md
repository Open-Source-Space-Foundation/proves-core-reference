# USP v1.0.0 to v1.2.1 delta, as it affects UspRadio and the OSSF forks

Research for [#587](https://github.com/Open-Source-Space-Foundation/proves-core-reference/issues/587) (map [#584](https://github.com/Open-Source-Space-Foundation/proves-core-reference/issues/584)). Date 2026-10-16.

## Answer

- **UspRadio still compiles against v1.2.1.** All 40 USP signatures and enum values that UspRadio uses are byte-identical between the two tags (measured).
- **Fork commits.** Three are obsolete because v1.2.1 deletes the files they patch: `dev_env.cmake`, the xiao `board.yml`, and the LR-FHSS path. Four are still needed: the NSS settle, the RAC mutex, the Kconfig select removal and the failsafe UNLOCK exemption. One is still needed and conflicts textually: the RF-switch GPIOs. The failsafe and RF-switch conflicts are small.
- **Two v1.2.1 changes hit PROVES.** They are a new required DT property `tx-power-cfg`, and a radio-planner change that makes the USP thread busy-loop while UspRadio holds a RAC lock (inference, not observed on hardware).
- **Zephyr version.** v1.2.1 pins Zephyr v4.4.0 and carries `< 4.3` and `< 4.4` version guards. PCR's Zephyr 4.4.2 is in range (inference: a patch release of 4.4).

## Sources

| Source | Ref |
|---|---|
| Lora-net/usp_zephyr | `v1.0.0` (bfacd43), `v1.2.1` (64f416b, a single squashed "Release v1.2.1" commit) |
| Lora-net/usp | `v1.0.0`, `v1.2.1` (540bba0, squashed) |
| OSSF usp_zephyr fork | `v1.0.0...0bf3e20` (6 commits + merges) |
| OSSF usp fork | `v1.0.0...8a60e4a` (2 commits + merges) |
| fprime-zephyr UspRadio | `9e3bedc`, `Drv/UspRadio/RalSessionImpl.{cpp,hpp}` |
| PCR USP board config | PR #439 head `1560dce`, `boards/bronco_space/proves_flight_control_board_v5e/*` |
| Zephyr | `v4.4.2` `modules/Kconfig`, `modules/lora-basics-modem/Kconfig`, `west.yml` |
| Changelog | `Lora-net/usp_zephyr@v1.2.1:CHANGELOG.md` (v1.1.2 and v1.2.1 sections) |

Upstream history between the tags is squashed, so this note cites file diffs at the tags, not individual upstream commits.

## Per-fork-commit verdict

The verdict for each commit comes from applying its diff to a `v1.2.1` worktree with `git apply --check`, then `git apply -3`.

| Fork commit | What it does | v1.2.1 status | Apply result on v1.2.1 | Verdict |
|---|---|---|---|---|
| usp_zephyr `a3a03c5` RF-switch GPIOs | `tx-enable-gpios`/`rx-enable-gpios` DT props. The pins toggle on the SetTx/SetRx/SetCad/SetSleep/SetStandby opcodes in `sx126x_hal_write` | No external RF-switch support. Only `dio2-as-rf-switch` exists (`drivers/usp/sx126x/sx126x_board.c:335-382`, `ral_sx126x_bsp_get_rf_switch_cfg`) | `sx126x_hal.c`, `sx126x_hal_context.h` and the binding apply cleanly. **`sx126x_board.c` conflicts**: v1.2.1 rewrote the `SX126X_CONFIG` macro for `SX126X_SPI_SPEC` and `pa_cfg_table` | **Needed, conflicts** (re-add the 2 `CONFIGURE_GPIO_IF_IN_DT` lines to the new macro) |
| usp_zephyr `1b9ad75` NSS wake settle | `k_busy_wait(500)` after the wake NSS pulse, before polling BUSY | `drivers/usp/sx126x/sx126x_hal.c` is **unchanged** v1.0.0 to v1.2.1 (`git diff --quiet` is true) | Clean | **Needed** |
| usp_zephyr `bbdd729` RAC mutex | Recursive `k_mutex` in `smtc_modem_hal_protect/unprotect_api_call` | Still stubs at `modules/smtc_modem_hal/smtc_modem_hal.c:184-192` ("Do nothing in case implementation is bare metal") | Clean | **Needed**. Alternative: `CONFIG_USP_THREADS_MUTEXES` plus wrapping every UspRadio RAC call in `SMTC_SW_PLATFORM()` (`include/zephyr/usp/smtc_sw_platform_helper.h:73-87`). That option already existed in v1.0.0, so it is not new |
| usp_zephyr `35f734b` Kconfig select removal | Drops `select ZEPHYR_LORA_BASICS_MODEM_MODULE` | Still present at `drivers/usp/Kconfig:12` | Clean | **Needed for PCR**. In Zephyr v4.4.2 the symbol lives in `modules/lora-basics-modem/Kconfig`. PCR's `west.yml` `name-allowlist` omits the `lora-basics-modem` project, so that Kconfig is not sourced (inference from Zephyr's module mechanism. Upstream v1.2.1 imports the full Zephyr manifest, so it never hits this) |
| usp_zephyr `93c6ec7` dev_env.cmake | Points `LR_FHSS_SRC_PATH` at the flat `sx126x_driver/src` | `modules/usp_drivers/dev_env.cmake` **deleted**. `sx126x.cmake:22` already defaults to `${LBM_SX126X_LIB_DIR}`, and `lr_fhss_mac.c` sits flat in `usp@v1.2.1:smtc_rac_lib/radio_drivers/sx126x_driver/src/` | Fails (file missing) | **Obsolete** (drop it) |
| usp_zephyr `895e77b` xiao board.yml | Adds `full_name` for the Zephyr 4.4 schema | `boards/seeed/xiao_nrf54l15/` **deleted entirely**. v1.2.1 ships no board dirs, only shields | Fails (file missing) | **Obsolete** (drop it) |
| usp `9d0dccf` failsafe UNLOCK | Exempts `RP_TASK_TYPE_UNLOCK_RADIO_ACCESS` from the 128 s `RP_FAILSAFE` panic | The failsafe condition is unchanged and exempts only LOCK (`smtc_rac_lib/radio_planner/src/radio_planner.c:471-477`) | **Conflicts**, trivially: v1.2.1 inserted an immediate-access early return just above. Keep both | **Needed** (or replace it with `rp_disable_failsafe()`, whose signature is unchanged, per the #584 hypothesis) |
| usp `3ee6cd2` lazy-sleep hysteresis | Holds the radio in STDBY_XOSC for `RP_LAZY_SLEEP_DELAY_MS` instead of sleeping it | No equivalent. v1.2.1 rewrote the arbiter: `rp_task_launch_current` moved to the arbiter tail behind `radio_task_running[]`, a new `timer_armed`, and a commented-out sleep on abort | **Conflicts** in `radio_planner.c` (3 hunks) and `radio_planner.h` (1) | Out of scope for #584. A port is a rewrite, not a rebase |

## API changes that affect UspRadio

### Compile surface: no breaks (measured)

The `tools/check_api.sh` lever (see Reproduce) extracted each signature or enum below at both tags and diffed them. The diff came out identical.

- **RAL inline functions (29).** `ral_set_pkt_type`, `ral_set_rf_freq`, `ral_set_tx_cfg`, `ral_set_standby`, `ral_set_sleep`, `ral_set_tx`, `ral_set_rx`, `ral_set_tx_cw`, `ral_set_pkt_payload`, `ral_get_pkt_payload`, `ral_set_dio_irq_params`, `ral_get_and_clear_irq_status`, `ral_clear_irq_status`, `ral_set_lora_{mod,pkt}_params`, `ral_set_lora_sync_word`, `ral_set_gfsk_{mod,pkt,crc}_params`, `ral_set_gfsk_sync_word`, `ral_set_gfsk_whitening_seed`, `ral_get_{lora,gfsk}_rx_pkt_status`, `ral_reset`, `ral_init`, `ral_handle_{tx,rx}_done`, `ral_cfg_rx_boosted`, `ral_set_rx_tx_fallback_mode` (in `smtc_rac_lib/smtc_ral/src/ral.h`).
- **RAC (6).** `smtc_rac_init`, `smtc_rac_open_radio`, `smtc_rac_get_radio`, `smtc_rac_get_rp`, `smtc_rac_lock_radio_access`, `smtc_rac_unlock_radio_access` (in `smtc_rac_lib/smtc_rac_api/smtc_rac_api.h`).
- **Radio planner (5).** `rp_disable_failsafe`, `rp_task_enqueue`, `rp_task_abort`, `rp_callback`, `rp_init`.
- **Enum values UspRadio names.** `RAL_STATUS_OK`, `RAL_IRQ_*`, `RAL_PKT_TYPE_*`, `RAL_GFSK_*`, `RAL_LORA_SF5`, `RAL_LORA_BW_125_KHZ`, `RAL_STANDBY_CFG_XOSC`, `RAL_RX_TIMEOUT_CONTINUOUS_MODE`.
- **`smtc_rac_scheduler_config_t`.** Fields and order unchanged (`smtc_rac_lib/smtc_rac/smtc_rac.h:470-481` vs `:361-372` at v1.0.0).
- **`rp_status_t`.** `RP_STATUS_REQUEST_NEXT_TX_PAYLOAD` is appended at the end (`radio_planner_types.h:242`), so existing values do not shift.
- **`zephyr_usp_initialization_wait`.** Unchanged (`usp_zephyr@v1.2.1:include/zephyr/usp/smtc_zephyr_usp_api.h:52`).
- **Additions UspRadio does not need.** `smtc_rac_get_callback_radio_id`, `smtc_rac_immediate_radio_access`, `smtc_rac_{get,set}_context_private`, `smtc_rac_context_t.keep_radio_awake`, and `pre_release`/`metadata` in `smtc_usp_rac_version_t`.

### Build and devicetree breaks for the v5e board

1. **`tx-power-cfg` is now `required: true`** (`dts/bindings/usp/semtech,sx126x-new-common.yaml:113-120`). The v5e node in PR #439 (`proves_flight_control_board_v5e_rp2350a_m33.dts:43-56`) has no such property, so the DT build fails until one is added (measured on the binding; build not run).
   - v1.2.1 also changed `ral_sx126x_bsp_get_tx_cfg` (`drivers/usp/sx126x/sx126x_ral_bsp.c:77-110`). It no longer hard-codes the SX1262 PA config (`hp_max=0x07`, `pa_duty_cycle=0x04`, configured power = requested power). It indexes `pa_cfg_table[power + 9]` over -9..22 dBm.
   - To reproduce v1.0.0 TX behaviour exactly, give 32 rows `<(p) 0x04 0x07>` for p = -9..22. Row order is power, pa_duty_cycle, hp_max (`sx126x_hal_context.h:56-61`).
   - Semtech's devkit table (`boards/shields/semtech_sx126xmbxxxs/semtech_sx1262_pa_cfg.dtsi`) is tuned for its own matching network, not the E22-400M30S external PA.
   - Inference: nothing checks the table length, so a short table reads out of bounds.
2. **TCXO derivation changed.** `SX126X_XOSC_CFG` now picks TCXO when `tcxo-wakeup-time` is *present*. v1.0.0 picked it when the value was non-zero (`sx126x_board.c:355-359`). The v5e node sets `<10>`, so nothing changes for v5e. A board that used `<0>` to mean "crystal" would flip to TCXO (inference).
3. **`hp_max` rename.** `sx126x_pa_pwr_cfg_t.pa_hp_sel` became `hp_max` (`sx126x_hal_context.h:60`). This is internal to usp_zephyr. UspRadio does not touch it.

### Runtime behaviour changes in the radio planner (bench re-test needed)

These come from `git diff v1.0.0 v1.2.1 -- smtc_rac_lib/radio_planner/src/radio_planner.c`. None of them was exercised on hardware for this note.

1. **The USP thread may spin while a LOCK is held (inference, high impact).**
   - v1.2.1 `rp_get_irq_flag` returns true whenever `tasks[radio_task_id].type == RP_TASK_TYPE_LOCK_RADIO_ACCESS` (`radio_planner.c:600-612`).
   - `rp_task_free` does not clear `type` (`:654`, commented out).
   - `zephyr_usp_thread.c:136-139` does `if (smtc_rac_is_irq_flag_pending()) continue;` and skips the sleep. That check is unchanged from v1.0.0 `:131`.
   - UspRadio holds a LOCK for every apply, RX, TX and CW, and keeps one open for the whole continuous-RX window. So the USP thread (default `CONFIG_USP_MAIN_THREAD_PRIORITY=2`, which PR #439 does not override) would re-run the engine without sleeping for as long as RX is armed. That can starve lower-priority F´ threads.
   - Verify on the bench with thread CPU stats before adopting v1.2.1. Possible mitigations are a lower USP thread priority or a local patch.
2. **The LOCK path reorders callback and arbiter (inference: affects UspRadio's completion semaphores).**
   - `rp_callback` now clears `radio_irq_flag` *before* `rp_hook_callback`.
   - It sets `arbiter_flag` around the callback, which suppresses the arbiter inside `rp_task_enqueue`.
   - It then runs the arbiter unless the task was retyped to UNLOCK (`:508-524`).
   - `rp_task_launch_current` now runs once, from the arbiter tail, gated by `radio_task_running[]` (`:903-910`). It used to run inline at each RUNNING transition.
   - These changes move when UspRadio's pre-callback runs relative to `smtc_rac_lock_radio_access` returning. They also move when the hook reaches `RP_TASK_STATE_FINISHED`, which `RalSessionImpl.cpp:396-407` and `:554-563` poll.
3. **Preemption no longer sleeps the radio.** The abort-for-priority path no longer calls `ral_set_sleep` (`:829`, commented out). The radio stays in STDBY_XOSC.
4. **`RAL_IRQ_RX_LEN_ERROR` now maps to `RP_STATUS_RX_CRC_ERROR`** (`:1021-1024`). UspRadio's CRC-error handling would see these too.
5. **Hook IDs shift by one.** `RP_HOOK_RAC_FLRP_MAC` is inserted before `RP_HOOK_RAC_HIGH_PRIORITY` (`radio_planner_hook_id_defs.h`). UspRadio uses the id that `smtc_rac_open_radio` returns, so it is unaffected (checked: no literal hook ids in `Drv/UspRadio/*.cpp`).
6. **`smtc_modem_hal_reset_mcu` and `smtc_modem_hal_on_panic` are now `_Noreturn`** in `usp@v1.2.1:smtc_rac_lib/smtc_modem_hal/smtc_modem_hal.h`. The usp_zephyr v1.2.1 implementations are not annotated but do end in a reset (`modules/smtc_modem_hal/smtc_modem_hal.c:126,255-277`).
   - The header also declares the new `smtc_modem_hal_check_modem_fifo_irq`. No `.c` in usp or usp_zephyr v1.2.1 calls or defines it (measured by `git grep`).

## Zephyr version

| | Zephyr pin in `west.yml` | Guards in code |
|---|---|---|
| usp_zephyr v1.0.0 | `v4.2.0`, usp `main` (unpinned) | none |
| usp_zephyr v1.2.1 | `v4.4.0`, usp `v1.2.1` | `ZEPHYR_VERSION_CODE < 4.3.0` for `SPI_DT_SPEC_GET` arity (`sx126x_board.c:370-374`). `< 4.4.0` for `fs/nvs.h` vs `kvss/nvs.h` (`smtc_modem_hal.c:43-48`) |
| PCR main | `v4.4.2` | n/a |

The changelog says "Zephyr 4.4 support (validated with SDK 1.0.1)". PCR's 4.4.2 is a patch release on the same minor version, so it should build (inference, no build run).

## Reproduce

```sh
# fork commit diffs
gh api repos/Open-Source-Space-Foundation/usp_zephyr/commits/<sha> -H "Accept: application/vnd.github.diff" > uz_<sha>.diff
# apply test against v1.2.1
git -C usp_zephyr worktree add --detach ../apply/usp_zephyr v1.2.1
git -C ../apply/usp_zephyr apply --check uz_<sha>.diff || git -C ../apply/usp_zephyr apply -3 uz_<sha>.diff
# API surface: extract each UspRadio-used prototype/enum at both tags and diff
# (protos.py regex-extracts "<ret> <name>( ... )" from a header at a tag)
for t in v1.0.0 v1.2.1; do python3 -I protos.py usp $t smtc_rac_lib/smtc_ral/src/ral.h ral_set_tx_cfg ... > p_$t; done; diff p_v1.0.0 p_v1.2.1
```

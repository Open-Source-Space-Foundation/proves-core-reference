# Radio + Watchdog Integration Test Failures — Debug Plan

> Branch: `create-v5e-device-tree`. CI infra is now solid (see [[reference_ci_cpu_probe]] in memory). Two real firmware failures remain on v5e hardware.

## Status snapshot

CI run that exposed these (after CI infra fixes): `gh run view 25713438576 --log` (`integration` job → `Run Integration Tests` step).

| Step | Outcome |
|---|---|
| build / lint / unit-test | ✓ |
| install MCUBoot + signed app, reboot, GDS start, **Sync Sequence Number** | ✓ |
| **Run Integration Tests** | ✗ (radio + watchdog) |

So everything boots, USB CDC ACM links up, GDS gets `CMD_NO_OP` acks. Comms path works.

## Failure 1 — Radio `test_01_transmit_enabled`

```
PROVESFlightControllerReference/test/int/0_radio_test.py:45
>   assert result is None
E   assert <EventData ...> is None

GDS EVR: ReferenceDeployment.lora.SendFailed (WARNING_HI):
  "Failed to send LoRa message: -11"
```

`-11` = `-EAGAIN`. Means the LoRa send attempt returned "would block / not ready" within the 10s window after enabling transmit.

### Recent commits to suspect (newest first)

```
729aa0e Reverting to See if TXCO is the Problem
51aa726 Add Include Statement for SX126X Header
63484ef add radio test
abc473f Add TXCO Stuff in the Device Tree
0680a8d Removed GPIO Pulldown for BUSY and DIO0
```

The v5e DT change in `boards/bronco_space/proves_flight_control_board_v5e/proves_flight_control_board_v5e_rp2350a_m33.dts:21-37` overrides `&lora0` for the SX1262 module (E22-400M30S). Note that **`dio3-tcxo-voltage` and `tcxo-power-startup-delay-ms` are currently commented out** (the `729aa0e` revert) — this is the open question the user was experimenting with.

### Hypotheses (rank)

1. **TXCO not powered on / not stabilized.** Without `dio3-tcxo-voltage` + startup delay, the SX1262 may be expecting an external TCXO that isn't powered, so it stays in `BUSY` after CMD issuance → driver returns `-EAGAIN`. The "Reverting TXCO" commit suggests the user is unsure whether the TXCO setting helps or hurts.
2. **`busy-gpios` / `dio1-gpios` polarity or pin assignment wrong on v5e.** The Zephyr `sx126x` driver polls BUSY before issuing commands and waits for DIO1 for TX-done. If polarity is inverted, the driver thinks the chip is permanently busy (→ EAGAIN) or never sees TX-done.
3. **`rx-enable-gpios` / `tx-enable-gpios` wiring** — these control external RF switches on E22 modules. Misconfig → chip-level command succeeds but no RF path → no completion event in time. Less likely to produce EAGAIN specifically, but worth eliminating.
4. **Power-amp configuration removed (`/delete-property/ power-amplifier-output`)** — SX1262 vs SX1276 driver bindings differ; the delete is correct for SX1262 but verify the `sx126x` driver doesn't require *something* that was on the parent SX1276 node.

### Investigation steps

1. **Compare DT against the E22-400M30S datasheet + SX1262 reference design.** Confirm:
   - DIO3 vs DIO2 used for TCXO control (SX1262 uses DIO3).
   - GPIO numbers match v5e schematic. The diff added: reset=GPIO0_6, busy=GPIO0_13, dio1=GPIO0_14, tx_en=GPIO0_21, rx_en=GPIO0_22. Confirm these match the v5e PCB.
2. **Re-enable TCXO config + add startup delay** and re-run. The values to try first:
   ```dts
   dio3-tcxo-voltage = <SX126X_DIO3_TCXO_1V8>;  // or 2V2 / 3V3, depends on module
   tcxo-power-startup-delay-ms = <10>;          // E22 datasheet typically 5-10ms
   ```
   The E22-400M30S uses a 1.8V TCXO typically; verify.
3. **Live-attach gdb during the failing test** to inspect the SX126x driver state. With the CI rack idle, use [[reference_gdb_pico_probe]] locally OR add a one-off `Probe CPU State` step to CI gated on `Run Integration Tests` failure (see [[reference_ci_cpu_probe]] for the skeleton — uncomment, scope to the right step). Look for:
   - PC inside `sx126x.c:sx126x_busy_wait` or `sx126x_send`
   - The driver's `dev_data->dio1` semaphore state
4. **Add temporary RTT / Logger output** in the FSW component that calls LoRa send — log the exact return code path. The driver's `-11` could come from `gpio_pin_get_dt(BUSY)` returning the wrong value or from `k_sem_take` timing out. Knowing which narrows hypothesis 1 vs 2.
5. **Bisect:** push a test build with TCXO re-enabled at last known-good values (look for prior `v5/v5c/v5d` boards that worked with similar E22 modules — check git history of those board defconfigs/DT for TCXO config that shipped successfully).

### Files to start with

- `boards/bronco_space/proves_flight_control_board_v5e/proves_flight_control_board_v5e_rp2350a_m33.dts:21-37` — the `&lora0` override (TCXO commented out here)
- `boards/bronco_space/proves_flight_control_board_v5/proves_flight_control_board_v5.dtsi` — base `lora0` definition the v5e overrides
- `lib/zephyr-workspace/zephyr/drivers/lora/loramac_node/sx126x.c` — Zephyr SX126x driver (look at `sx126x_transmit` and busy-wait code)
- `lib/zephyr-workspace/modules/lib/loramac-node/src/radio/sx126x/radio.c` — LoRaMac-node Radio layer
- `PROVESFlightControllerReference/project/config/LoRaCfg.hpp` — FSW LoRa config (1 line changed on branch)

## Failure 2 — Watchdog `test_03_system_reboots_without_watchdog`

```
PROVESFlightControllerReference/test/int/watchdog_test.py:115
> assert final_boot_count == initial_boot_count + 1
E   AssertionError: Initial boot count: 26, Final boot count: 26
```

Test stops the watchdog, waits 60s, expects boot count to have incremented by 1 (because watchdog should have triggered a reset). It did not. So either:

a) Watchdog never tripped (timer config wrong / fed from a path that didn't stop).
b) Watchdog tripped but didn't actually reset the chip.
c) Reset happened but boot count didn't increment (`BootCounter` component not persisting / not reading correctly on v5e flash layout).

### Hypotheses

1. **MCUBoot's boot count storage region overlap with v5e flash partition layout.** The v5e defconfig sets `CONFIG_ROM_END_OFFSET=0x1150` and `CONFIG_MCUBOOT_UPDATE_FOOTER_SIZE=0x1000`. If the boot count is stored in a settings partition, those offsets matter.
2. **Watchdog HW peripheral not enabled in v5e DT.** Compare `&watchdog0 { status = "okay"; }` (or whatever the node is) between v5d and v5e DT — easy diff.
3. **Watchdog `STOP_WATCHDOG` command path actually disables more than intended on v5e** (e.g., disables the timer that would later trigger reset). Less likely.

### Investigation steps

1. `diff` v5d vs v5e DTS for watchdog-related nodes / `chosen` entries.
2. Run only `watchdog_test.py::test_03_system_reboots_without_watchdog` locally with serial logging; observe whether `WatchdogStop` event fires (it does per CI log), then observe whether system stays alive past the 30s watchdog window. If alive → watchdog not actually tripping. If dead but boot count stuck → BootCounter persistence issue.
3. Check `PROVESFlightControllerReference/Components/BootCounter` (or wherever boot count is kept) for storage backend.

### Lower priority than radio

This may be a v5e flash-partition issue rather than a radio change. Tackle radio first since most recent commits target it. If watchdog still fails after radio is fixed, dig into flash partitions.

## Workflow recommendations for the next agent

- **Don't dig CI-side again unless infra regresses.** The infrastructure issues (dual-board USB, MCUBoot board mismatch, UART_DEVICE plumbing) are documented and fixed. See memory entries.
- **Use [[reference_gdb_pico_probe]] for local debug** — much faster iteration than CI roundtrip. Local dev box has the Pico Probe + OpenOCD fork ready at `/Users/ncc-michael/GitHut/openocd`.
- **Push to CI only after local confirmation** — each CI run consumes the integration runner for ~5 min and we already burned a lot of cycles.
- If you need fresh diagnostics from CI on the actual radio failure, paste in the `Probe CPU State` step pattern from [[reference_ci_cpu_probe]] with `if: failure() && steps.<step_id>.outcome == 'failure'` scoped to `Run Integration Tests`. Remove it after extracting the data.

## Once green

- Delete this file (`RADIO_DEBUG_PLAN.md` is intended as a transient handoff doc, not committed long-term).
- If the v5d board can be physically removed from the CI rack, do that and the sysfs VID:PID detect becomes redundant — could revert to the simpler "first non-probe device" approach. Until then, keep the VID:PID detect (it's idempotent and stable).

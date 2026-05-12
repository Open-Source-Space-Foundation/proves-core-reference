# Radio + CI Handoff — status as of 2026-05-12

> Branch: `create-v5e-device-tree`. Radio is **working**. Smoke test confirms
> end-to-end LoRa downlink via the v5d passthrough. One unrelated integration
> failure remains (drv2605 magnetorquer power draw).

## What changed and why

| Commit | Purpose |
|---|---|
| `b087002` | Enable SX1262 TCXO (1.8V, 10 ms startup) in v5e DT. Fixes `SendFailed -EAGAIN` (-11): module needed `dio3-tcxo-voltage` + `tcxo-power-startup-delay-ms` to leave BUSY. |
| `ae4fc9e` | Fix `0_radio_test::test_01_transmit_enabled` — swap `assert_event` (raises on miss) for `await_event` (returns None). Also covers `ConfigurationFailed` / `AllocationFailed`. |
| `c21ecc4` | Mark `watchdog_test::test_03_system_reboots_without_watchdog` with `requires_hw_watchdog`; skip by default. v5e wired board has no HW watchdog enabled. |
| `6232cb6` | Add LoRa passthrough downlink smoke test (`1_lora_passthrough_test.py`). CI auto-detects v5d's data CDC (CircuitPython "CDC2 control" iInterface). |

### CI auto-detect (`Detect LoRa Passthrough TTY` step)

Iterates `/sys/bus/usb/devices/*`, skips Zephyr (`0028:000f`), picks CDC ACM
control interface whose `iInterface` matches `*cdc2*` / `*data*`. On the
runner this picks `/dev/ttyACM4` (`1209:e004 PROVESKit RP2350 V5d`).

### Reset-manager gotcha

`reset_manager_test` runs cold/warm reset → FSW boots with `TRANSMIT=DISABLED`
and downlink divider at default. Anything that relies on the radio MUST run
before `reset_manager_test.py` alphabetically OR re-issue
`downlinkDelay.DIVIDER_PRM_SET` + `lora.TRANSMIT ENABLED` and give the radio
~2 s to settle. The smoke test is named `1_*` so it runs right after
`0_radio_test`.

## Open work — `drv2605_test::test_01_magnetorquer_power_draw`

```
AssertionError: Power during magnetorquer trigger (1.7934 W) not sufficiently
above baseline power (3.3184 W)
```

Triggered magnetorquer should *increase* power; instead it dropped. Baseline
already high (3.3 W) which is suspicious. Possible causes:

1. **Magnetorquer not actually firing** — check Drv2605 / MCP23017 GPIO line
   (`fire_deploy2_b` or relevant face_enable). v5e moved MCP from I2C1 to
   I2C0; verify line names still resolve.
2. **Power monitor reading swapped** — INA219 channels (`ina219_0` sys vs
   `ina219_1` sol) on v5e may not match what test expects.
3. **Baseline already includes magnetorquer current** — wiring/topology
   change on v5e left it always-on.

Start by reading `PROVESFlightControllerReference/Components/Drv2605Manager/`
and the test in `PROVESFlightControllerReference/test/int/drv2605_test.py`,
then compare the v5e DT (`boards/bronco_space/proves_flight_control_board_v5e/`)
against v5d to identify what wiring/binding changed.

## Verifying current green state

- Latest CI on branch: filter by `gh run list --branch create-v5e-device-tree`.
- Last full radio+smoke-passing run: `25715681808` (radio test `.` smoke `.`,
  only drv2605 failed).

## Resources

- [[reference_gdb_pico_probe]] — local CPU halt without flash/reset.
- [[reference_ci_cpu_probe]] — drop-in CI step pattern for live state on
  integration failure.
- [[project_ci_dual_board_gotcha]] — VID:PID `0028:000f` is the Zephyr board;
  everything else is the v5d passthrough.

## Once green

Delete this file. It's a transient handoff.

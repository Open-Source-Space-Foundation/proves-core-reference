# Drv::I2cBusMonitor

The I2C Bus Monitor component recovers I2C0 when a target holds SDA or SCL low. This can occur when face power is cut during a transfer. Without recovery, every I2C0 consumer times out until a power cycle (issue #540).

## Triggers

1. The 1 Hz `run` port, when SDA or SCL reads low.
2. The `RECOVER_BUS` command, which recovers even when both lines read high.

The Zephyr `i2c_dw` driver can also call the callback from its transfer path after an SDA-stuck abort. The RP2350 DesignWare IP does not report that abort, so this path does not run on this board.

## Recovery Sequence

`configure()` registers the callback with `i2c_dw_register_recover_bus_cb()`. Both triggers call `i2c_recover_bus()`, which runs the callback while it holds the driver `bus_sem`, so no transfer is on the wire. The callback:

1. Reads SDA and SCL. If both are high and the recovery is not forced, it stops.
2. Sets SDA and SCL as open-drain GPIOs.
3. Clocks SCL up to 9 times until SDA is released, then sends a STOP. The mux channel is still connected, so a target behind the mux also completes its byte.
4. Pulses the TCA9548A reset (GP26). This disconnects all channels and clears a mux that is stuck mid-byte. The `tca954x` driver sets this line only at init.
5. If a line is still low, does step 3 again for targets on the trunk.
6. Applies the I2C0 default pinctrl state and reads the final line levels. The recovery is successful if both lines are high.

## Back-off

Each automatic recovery takes less than 1 ms, but a bus that stays stuck makes every I2C0 transfer time out. After 5 automatic recoveries with no clean period, the component emits `AutoRecoveryBackOff` once and the 1 Hz poll recovers at most once every 30 s. Ten consecutive polls with both lines high end the back-off. `RECOVER_BUS` is not limited.

`BusStuck`, `BusRecovered` and `BusRecoveryFailed` have a throttle of 5. The same ten clean polls clear the throttles.

## Design Notes

- **Stale mux channel.** The `tca954x` driver caches the selected channel. With `i2c-mux-idle-disconnect` the cache is 0 after each transfer and agrees with the mux after a reset. If the disconnect write failed, the next transfer on that channel fails one time, and then the driver is in sync again.
- **Controller not disabled.** `i2c_dw` has no public API to disable the controller under `bus_sem`. The controller is idle while the pins are GPIOs, and the next transfer disables and enables it in `i2c_dw_setup()`.
- **Pinctrl access.** `APP_I2C_BUS_RECOVERY` (promptless, default y) selects `PINCTRL_NON_STATIC`, so `Main.cpp` can get the `i2c0` pinctrl configuration.
- **Thread safety.** `m_lock` serializes the rate group and command paths. The callback writes the line levels before `bus_sem` is released.
- **Power cuts.** The face managers use guarded ports, and `LoadSwitch` notifies them before it removes power. Thus an OFF waits for an in-flight transfer on that face, which prevents most stuck buses. This component recovers from the rest.

## Interface

See `I2cBusMonitor.fpp` for the ports, commands, events and telemetry.

## Requirements
| Name | Description | Validation |
|---|---|---|
| Automatic recovery | The component shall recover I2C0 when the 1 Hz poll finds a line low | HIL wedge test (SWD) |
| Commanded recovery | The component shall recover I2C0 on `RECOVER_BUS` | HIL test `i2c_bus_monitor_test.py` |
| Back-off | After 5 automatic recoveries with no clean period, the component shall recover at most once every 30 s | HIL wedge test (SWD) |

## Change Log
| Date | Description |
|---|---|
| 2026-10-04 | Initial I2C Bus Monitor component SDD |

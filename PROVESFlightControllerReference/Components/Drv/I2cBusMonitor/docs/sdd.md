---
title: I2C Bus Monitor
category: Driver
---

# Drv::I2cBusMonitor

The I2C Bus Monitor component recovers I2C0 when a target holds SDA or SCL low. This can occur when face power is cut during a transfer. Without recovery, every I2C0 consumer times out until a power cycle (issue #540).

## Triggers

1. A transfer that times out. The Zephyr `i2c_dw` driver calls the callback from its timeout path (`patches/zephyr-i2c-dw-timeout-recover-bus.patch`), before the next transfer. This covers a controller stalled with bytes in its TX FIFO: both lines read high, so no line check sees it, and every transfer times out at 100 ms. On EM Orange Case 001 a face power-on glitch caused it. The driver aborts a timed-out transfer, which flushes the FIFO, so a poll of `IC_STATUS` misses it too. The callback runs on the thread that did the transfer, so it only counts the result. The next 1 Hz poll emits `TimeoutRecoveries` and updates the telemetry.
2. The 1 Hz `run` port, when SDA or SCL reads low.
3. The `RECOVER_BUS` command.

The driver also calls the callback at the start of a transfer after an SDA-stuck abort. The RP2350 DesignWare IP does not report that abort, so this path does not run on this board. If it did, `TimeoutRecoveries` would count it.

## Recovery Sequence

`configure()` registers the callback with `i2c_dw_register_recover_bus_cb()`. The driver runs the callback while it holds `bus_sem`, so no other transfer is on the wire. The 1 Hz poll and the command call `i2c_recover_bus()`, which takes `bus_sem` and runs the callback. The callback always runs the full sequence:

1. Reads SDA and SCL, for `BusStuck`.
2. Aborts and disables the DesignWare controller (`IC_ENABLE.ABORT`, then `IC_ENABLE = 0` and a poll of `IC_ENABLE_STATUS`, 10 ms limit each). This releases SCL when the controller itself holds it for an unread byte, and flushes both FIFOs.
3. Sets SDA and SCL as open-drain GPIOs.
4. Clocks SCL up to 9 times until SDA is released, then sends a STOP. The mux channel is still connected, so a target behind the mux also completes its byte.
5. Pulses the TCA9548A reset (GP26). This disconnects all channels and clears a mux that is stuck mid-byte. The `tca954x` driver sets this line only at init.
6. If a line is still low, does step 4 again for targets on the trunk.
7. Applies the I2C0 default pinctrl state, enables the controller and reads the final line levels. The recovery is successful if both lines are high.

## Recovery Details

Each recovery records the controller registers before the sequence starts (`IC_TAR`, `IC_STATUS`, `IC_RAW_INTR_STAT`, `IC_TX_ABRT_SOURCE`, `IC_TXFLR`, `IC_RXFLR`), the TCA954x channel the driver last selected, the SDA and SCL levels after each step, the clocks sent, and the result. For a driver timeout it also records the driver state (`IC_INTR_MASK` before the driver masked it, `xfr_len`, `rx_pending`, `request_bytes`, `state`, `xfr_flags`, messages left), emitted as `RecoveryDriverState`. The driver patch calls the callback before its own abort, so the registers still show the stuck transfer. The records go into a 16-slot ring. The 1 Hz poll emits up to 8 per cycle as `RecoveryDetail`. If the ring is full, `RecoveryDetailsDropped` counts the lost records.

`SET_RECOVERY_STEPS` selects which steps run, so a bench test can find the step that clears a fault: 1 halt the controller, 2 clock out while SDA is low, 4 always clock 9 times, 8 pulse the mux reset. The default is 11 (halt, clock out, mux reset). The setting is lost on reboot.

## Back-off

Each recovery takes less than 1 ms, but a bus that stays stuck makes every I2C0 transfer time out. After 5 automatic recoveries from the 1 Hz poll with no clean period, the component emits `AutoRecoveryBackOff` once and the poll recovers at most once every 30 s. Ten consecutive clean polls (both lines high, no timeout recoveries) end the back-off. `RECOVER_BUS` is not limited.

The timeout trigger has no back-off. It adds less than 1 ms to a transfer that already waited 100 ms.

`BusStuck`, `BusRecovered`, `BusRecoveryFailed` and `TimeoutRecoveries` have a throttle of 5. The same ten clean polls clear the throttles.

## Design Notes

- **Stale mux channel.** The `tca954x` driver caches the selected channel. With `i2c-mux-idle-disconnect` the cache is 0 after each transfer and agrees with the mux after a reset. If the disconnect write failed, the next transfer on that channel fails one time, and then the driver is in sync again.
- **Controller reset.** `i2c_dw` has no public API to reset the controller, so the component writes `IC_ENABLE` directly while it holds `bus_sem`. On EM Orange Case 001 the controller held SCL low for an unread byte after a timed-out read. A disable alone never completed. ABORT released SCL within 5 ms, and the disable then completed.
- **Pinctrl access.** `APP_I2C_BUS_RECOVERY` (promptless, default y) selects `PINCTRL_NON_STATIC`, so `Main.cpp` can get the `i2c0` pinctrl configuration.
- **Thread safety.** `m_lock` serializes the rate group and command paths. The callback must not take `m_lock`: a timed-out transfer holds `bus_sem`, and `run_handler` takes `m_lock` before `bus_sem`. The callback tells the two apart by thread: `recover()` stores its thread before it calls `i2c_recover_bus()`. Timeout recoveries go into atomic counters. A timeout recovery on another thread can overwrite the line levels between the callback and the `BusStuck` event of the poll. The event can then show the wrong levels, which is acceptable.
- **Power cuts.** A face power cut during a transfer on that face is the known cause of a stuck bus (#540). This component recovers the bus after the cut. Preventing a power cut during a transfer is handled separately.

## Interface

See `I2cBusMonitor.fpp` for the ports, commands, events and telemetry.

## Requirements
| Name | Description | Validation |
|---|---|---|
| Automatic recovery | The component shall recover I2C0 when the 1 Hz poll finds a line low | HIL wedge test (SWD) |
| Timeout recovery | The component shall recover I2C0 after an I2C0 transfer times out | HIL face power-on test (EM) |
| Commanded recovery | The component shall recover I2C0 on `RECOVER_BUS` | HIL test `i2c_bus_monitor_test.py` |
| Back-off | After 5 automatic recoveries with no clean period, the component shall recover at most once every 30 s | HIL wedge test (SWD) |

## Change Log
| Date | Description |
|---|---|
| 2026-10-04 | Initial I2C Bus Monitor component SDD |
| 2026-10-27 | Recover a stalled controller (queued bytes, idle master), and abort and disable the controller during recovery. Both found on EM Orange Case 001 |
| 2026-10-27 | Record controller state, mux channel and line levels for each recovery; add `SET_RECOVERY_STEPS` |
| 2026-10-27 | Trigger recovery from the driver timeout path instead of polling `IC_STATUS`, which missed the stall on the EM |

# Testing without the flight controller

Run `bash modules/kleo-ina260/test-in-wsl.sh` from the KLEO repository in WSL.
The script uses the existing reference checkout and its `.venv-ina219` environment.
Set `KLEO_REFERENCE` to override `/home/asr/projects/proves-v5c-reference`.
It copies this module to a new temporary build directory; it never opens USB or flashes.

## Implemented and passing

- Production C core: conversion boundaries, identity checks, failures at every read,
  configuration readback, atomic sample publication, timeout, overflow and recovery.
- Actual Zephyr sensor driver on `native_sim/native/64`: five ztests exercise
  `sensor_sample_fetch` and `sensor_channel_get` through an emulated I2C controller.
  Checks include negative fractional current, voltage and power scaling, cached reads,
  invalidation after failure, write failures, recovery, conversion timeout, overflow,
  wrong device identity, unsupported channels and independent sensor instances.

Validated on Zephyr 4.4.2 with host GCC 13.3.0. All five native tests passed;
the core suite also passed with undefined-behavior sanitization enabled.
The emulator supplies register values and injected errors; it is not a physical
model of the INA260, a bus electrical model, or a CPU emulator for RP2350.

## Next layers (not implemented yet)

1. F Prime component tests: feed synthetic sensor samples into a future power-monitor
   component and check telemetry, error events, stale-data handling and recovery.
2. Software integration: connect simulated sensors to a host deployment and its
   generated dictionary. Check commands and decoded telemetry through GDS or Yamcs.
3. System scenarios: unplugged sensor, delayed readings, out-of-range readings,
   ground-station disconnect/reconnect and prolonged operation.

Each additional sensor needs either an emulator for its real driver or a mock at
the component interface. The latter tests application behavior but not the driver.
The current KLEO scaffold needs build fixes and host-compatible adapters before a
whole-system simulation is possible. No end-to-end system test is claimed here.

Hardware remains necessary to validate V5c wiring, pull-ups, addresses, real conversion
timing, measurement accuracy, power behavior, bootloader compatibility and watchdogs.

# PROVES Core Reference

F Prime flight software for the PROVES CubeSat, built on Zephyr RTOS for the RP2350 flight-control board. This glossary pins down the project's testing vocabulary; see `docs/adr/` for the decisions behind it.

## Language — Testing Tiers

**Helper Test**:
A gtest of an extracted, Zephyr-free, F Prime-free helper class (e.g. `BDot`, `RtcHelper`), built standalone in `PROVESFlightControllerReference/test/unit-tests/`.
_Avoid_: vanilla unit test, plain gtest

**Component UT**:
An F Prime unit test of a single component through its autocoded Tester base classes, built with a native (Linux/Darwin) toolchain and run via `fprime-util check`.
_Avoid_: unit test (alone — ambiguous with Helper Test)

**Ztest Lane**:
Zephyr ztest suites run by Twister on `native_sim`, testing Zephyr-calling helper code against in-tree emulated devices (`rtc_emul`, `flash_simulator`, `gpio_emul`, FATFS-on-ramdisk). Linux-only.
_Avoid_: Z Tests, Twister tests

**SIL**:
Software-in-the-loop: a Subsystem Deployment built for the host (posix), driven by fprime-gds pytest over TCP, with hardware replaced by Behavioral Models. Runs on macOS and Linux.
_Avoid_: SITL (reserved for the full-topology tier), simulation (unqualified)

**SITL Tier**:
The full ReferenceDeployment topology running on Zephyr `native_sim` with Reverse Drivers supplying scripted sensor/actuator behavior. Real OSAL, real rate groups, real topology wiring. Linux-only.
_Avoid_: full sim, native build (ambiguous with Component UT builds)

**HWIL**:
Hardware-in-the-loop: the existing self-hosted CI tier flashing real boards (`integration-uart`, `integration-radio`).
_Avoid_: integration tests (alone — every tier above Helper Test integrates something)

## Language — Simulation Parts

**Behavioral Model**:
An F Prime component that implements a hardware driver's port interface (e.g. `Svc.Com` for the radio) with scripted behavior and fault injection driven by F Prime commands/telemetry. Lives at the F Prime port boundary; used by SIL.
_Avoid_: stub (implies no behavior), mock

**RadioModel**:
The Behavioral Model that replaces `Zephyr.LoRa` in the radio Subsystem Deployment, including the `enableTransmit`/`disableTransmit`/`loraFirstStart` ports and fault-injection commands (drop, corrupt, delay, radio-busy, ALARM).

**Reverse Driver**:
A Zephyr-API-level fake device driver (e.g. implementing `sensor_driver_api`) returning scripted values, handed to manager components via their `configure()` device injection. Lives at the Zephyr driver boundary; used by the SITL Tier.
_Avoid_: emulator (reserved for register-level Chip Emulators)

**Chip Emulator**:
A register-level `i2c_emul` backend emulating a specific part (e.g. TMP112). Deliberately not built in this project (see ADR 0002).

**Subsystem Deployment**:
A small host-buildable F Prime deployment wiring one subsystem's real components plus Behavioral Models and a minimal command/event/telemetry core, for SIL testing.

# Components::StartupManager

## Overview

The StartupManager component manages boot counting, quiescence waiting periods, automatic dispatch of startup sequences, and an optional hard-coded LoRa transmit enable after a configurable countdown.

## Purpose

The StartupManager serves four primary functions:

1. Boot Counting: Tracks the number of system boots persistently across power cycles, hardened against file corruption from hard resets (see [Boot count persistence](#boot-count-persistence))
2. Quiescence Wait: Implements a configurable waiting period (default 45 minutes) before allowing full system startup, useful for missions requiring initial stabilization
3. Startup Sequence: Automatically dispatches and monitors the execution of startup command sequences
4. Hard-coded Radio Enable: When enabled at compile time (`DEFAULT_STARTUP_VALUE == 1` in `HardCodedStartup.h`), after `TRANSMIT_ENABLE_TICKS` 1 Hz run ticks, asserts `enableTransmit` to enable LoRa transmission independently of the startup sequence file. When disabled (`DEFAULT_STARTUP_VALUE == 0`), the countdown does not run and transmit is not asserted automatically — preferred for ground testing so the radio does not turn on accidentally.

### Diagrams
Add diagrams here

### Typical Usage

## How to Run

1. Choose the startup sequence from the sequences file. To update the .bin file run `make sequence SEQ=startup`
2. Upload the startup.bin file using uplink. Make sure its set in root in the cube as startup.bin
3. Restart the cube, it should do the startup sequence right away
4. To disable the startup sequence delete the sequence file. Use FileHandling.filemanager.RemoveFile to remove the startup.bin file

The StartupManager maintains internal state tracking its lifecycle:

| State | Description | Trigger |
|-------|-------------|---------|
| **Uninitialized** | Initial state before first `run` call. `m_boot_count == 0` | System initialization |
| **Initialized** | Boot count and quiescence start time have been loaded/set | First `run` call |
| **Waiting for Quiescence** | `m_waiting == true`, awaiting quiescence period expiration or disarm | `WAIT_FOR_QUIESCENCE` command received |
| **Running** | Normal operation, updating telemetry on each `run` call | Continuous after initialization |

**State Transitions:**

```
Uninitialized → Initialized (first run call)
Initialized → Waiting for Quiescence (WAIT_FOR_QUIESCENCE command)
Waiting for Quiescence → Running (quiescence period expires OR ARMED=false)
```

On each 1 Hz `run`, when `DEFAULT_STARTUP_VALUE == 1` and `m_transmit_enable_ticks > 0`, the counter decrements. When it reaches 0, StartupManager logs `HardcodedRadioEnable` and invokes `enableTransmit_out` if that port is connected. When `DEFAULT_STARTUP_VALUE == 0`, this path is inactive.

### Hard-coded startup compile-time gate

`HardCodedStartup.h` defines `DEFAULT_STARTUP_VALUE`:

### Boot count persistence

The boot count lives in `BOOT_COUNT_FILE` on the flight filesystem (FAT — ELM FatFs, see `prj.conf`) and is incremented lazily on the first 1 Hz `run` tick of each boot. Hard resets (e.g. the watchdog power cycle used for command-loss recovery) can land while a write is in flight, so the persistence path is hardened in four ways:

1. **Corruption guard**: a read that returns a value above 1,000,000 is treated as a failed read (the file contained torn/junk data) and reported via `BootCountCorrupted` with the raw value. The count then re-initializes on the next increment instead of propagating garbage. Observed on HWIL: a torn write left the file reading `0x02FE191005000001`, and the next boot persisted exactly garbage+1.
2. **Read-only queries**: `GET_BOOT_COUNT` never writes the file. Only the once-per-boot increment does, minimizing the window in which a reset can tear a write.
3. **Increment retry**: if the first-tick persist fails (e.g. filesystem not ready), `run` re-attempts it each tick until it succeeds — the increment is delayed, not lost. `BootCountUpdateFailure` is emitted once per failure streak.
4. **Write-then-rename persist**: the value is written to `<BOOT_COUNT_FILE>.tmp`, flushed, and renamed over the target. FAT's rename is not guaranteed power-cut atomic, but the new data is fully on storage before it replaces the old file, closing the torn-in-place-write window that produced the observed garbage. The residual worst case during the rename window is a missing file, which reads as a failed read (count re-initializes, visibly) rather than silent corruption.


## Port Descriptions

| Port Name | Type | Direction | Description |
|-----------|------|-----------|-------------|
| `run` | `Svc.Sched` | Input (sync) | Scheduled by the 1 Hz rate group. Boot init, quiescence monitoring, and transmit-enable countdown |
| `runSequence` | `Svc.CmdSeqIn` | Output | Dispatches the startup sequence to the command sequencer |
| `sequenceStarted` | `Svc.CmdSeqIn` | Input (sync) | Receives the filename of the sequence that started |
| `completeSequence` | `Fw.CmdResponse` | Input (sync) | Receives completion status from the command sequencer |
| `enableTransmit` | `Fw.Signal` | Output | Asserted once when the transmit-enable countdown expires and hard-coded startup is enabled; wired to LoRa |
| `disableTransmit` | `Fw.Signal` | Output | Disables LoRa transmission; wired to LoRa |

## Component States

| State Variable | Type | Description |
|----------------|------|-------------|
| `m_boot_count` | `FwSizeType` | Current boot count. Zero indicates uninitialized state |
| `m_boot_count_persisted` | `bool` | Whether the incremented boot count has durably reached the file; drives the per-tick retry |
| `m_boot_count_write_logged` | `bool` | `BootCountUpdateFailure` already emitted for the current persist-failure streak |
| `m_quiescence_start` | `Fw::Time` | Time when quiescence period started (mission epoch) |
| `m_waiting` | `std::atomic<bool>` | True when waiting for quiescence period to elapse |
| `m_stored_opcode` | `FwOpcodeType` | Opcode of pending `WAIT_FOR_QUIESCENCE` command |
| `m_stored_sequence` | `U32` | Sequence number of pending `WAIT_FOR_QUIESCENCE` command |
| `m_transmit_enable_ticks` | `U32` | Remaining 1 Hz ticks until hard-coded transmit enable; loaded from `TRANSMIT_ENABLE_TICKS` on first boot |

## Sequence Diagrams

## Parameters

| Name | Type | Default Value | Description |
|------|------|---------------|-------------|
| `ARMED` | `bool` | `true` | When true, system waits for quiescence period. When false, quiescence is bypassed |
| `QUIESCENCE_TIME` | `Fw.TimeIntervalValue` | `{seconds = 45 * 60, useconds = 0}` | Duration to wait for quiescence (45 minutes by default) |
| `QUIESCENCE_START_FILE` | `string` | `"/quiescence_start.bin"` | File path for storing the mission-wide quiescence start time |
| `STARTUP_SEQUENCE_FILE` | `string` | `"/startup.bin"` | Path to the command sequence file to run at startup |
| `BOOT_COUNT_FILE` | `string` | `"/boot_count.bin"` | File path for storing the boot count |
| `TRANSMIT_ENABLE_TICKS` | `U32` | `2800` | 1 Hz run ticks before enabling LoRa transmit (45×60 + 100 s margin) |

## Commands

| Name |  Description |
|------|-------------|
| `WAIT_FOR_QUIESCENCE` | Lets you start with opcode cmdseq and whether or not waiting |

## Events

| Name | Severity | Arguments | Description |
|------|----------|-----------|-------------|
| `CurrentBootCount` | ACTIVITY_LO | `i: I64` | Emitted by `GET_BOOT_COUNT` with the current boot count |
| `BootCountUpdateFailure` | WARNING_LO | None | Emitted once per failure streak when the boot count file cannot be updated. The increment is retried on each subsequent `run` tick until it persists |
| `BootCountCorrupted` | WARNING_HI | `raw: I64` | Emitted when the boot count file holds an implausible value (> 1,000,000), indicating a torn or corrupt write. The value is treated as unreadable |
| `QuiescenceFileInitFailure` | WARNING_LO | None | Emitted when the quiescence start time file cannot be initialized. System will use current time but cannot persist it |
| `StartupSequenceFinished` | ACTIVITY_LO | None | Emitted when the startup sequence completes successfully |
| `StartupSequenceFailed` | WARNING_LO | `response: Fw.CmdResponse` | Emitted when the startup sequence fails, includes the failure response code |
| `HardcodedRadioEnable` | ACTIVITY_HI | None | Emitted when hard-coded startup is enabled, the transmit countdown expires, and `enableTransmit` is asserted |

## Telemetry

| Name | Type | Update Policy | Description |
|------|------|---------------|-------------|
| `BootCount` | `FwSizeType` | Update on change | Current boot count. Increments on each system boot |
| `QuiescenceEndTime` | `Fw.TimeValue` | Update on change | Absolute time when the quiescence period will end. Updated on each `run` call |

## Requirements

| Requirement ID | Description | Validation Method |
|----------------|-------------|-------------------|
| REQ-SM-001 | StartupManager shall track boot count across power cycles | Verification: Check that boot count increments on each boot via telemetry |
| REQ-SM-002 | StartupManager shall support configurable quiescence waiting period | Verification: Confirm QUIESCENCE_TIME parameter affects wait duration |
| REQ-SM-003 | StartupManager shall automatically dispatch startup sequence on first run call | inspection |
| REQ-SM-004 | StartupManager shall allow disabling quiescence via `ARMED` parameter | Verification: Set `ARMED=false` and confirm `WAIT_FOR_QUIESCENCE` completes immediately |
| REQ-SM-005 | StartupManager shall emit events for sequence completion status | Verification: Monitor events during sequence execution |
| REQ-SM-006 | StartupManager shall update telemetry on each run cycle | Verification: Confirm `BootCount` and `QuiescenceEndTime` telemetry updates |
| REQ-SM-007 | StartupManager shall handle file I/O errors gracefully | Verification: Remove file permissions and verify warning events are emitted |
| REQ-SM-008 | When `DEFAULT_STARTUP_VALUE == 1`, StartupManager shall enable LoRa transmit after `TRANSMIT_ENABLE_TICKS` 1 Hz ticks | Verification: Confirm `HardcodedRadioEnable` and RF transmit after the configured delay |
| REQ-SM-009 | When `DEFAULT_STARTUP_VALUE == 0`, StartupManager shall not assert `enableTransmit` from the hard-coded countdown | Verification: Build with gate disabled and confirm no `HardcodedRadioEnable` / automatic TX after boot |
| REQ-SM-010 | StartupManager shall not propagate an implausible boot count read from a corrupt file | Verification: Write junk to `BOOT_COUNT_FILE`, reboot, confirm `BootCountCorrupted` and a re-initialized count |
| REQ-SM-011 | StartupManager shall persist the boot count atomically and retry a failed increment until it is durably stored | Verification: HWIL `test_safe_09` asserts boot count == initial+1 across a watchdog hard reset |

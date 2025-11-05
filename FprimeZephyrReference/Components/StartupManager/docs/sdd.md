# Components::StartupManager

## Overview

The StartupManager component manages boot counting, quiescence waiting periods, and automatic dispatch of startup sequences.

## Purpose

The StartupManager serves three primary functions:
1. Boot Counting: Tracks the number of system boots persistently across power cycles
2.  Implements a configurable waiting period (default 45 minutes) before allowing full system startup, useful for missions requiring initial stabilization
3. Automatically dispatches and monitors the execution of startup command sequences

## Usage Examples
Add usage examples here

### Diagrams
Add diagrams here

### Typical Usage

## How to Run

1. Choose the startup sequence from the sequences file. To update the .bin file run `make sequence SEQ=startup`
2. Restart the cube, it should do the startup sequence right away
3. To disable the startup sequence delete the sequence file. Use FileHandling.filemanager.RemoveFile to remove the startup.bin file

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

## Port Descriptions

| Port Name | Type | Direction | Description |
|-----------|------|-----------|-------------|
| `run` | `Svc.Sched` | Input (sync) | Scheduled execution port called by rate group. Manages boot initialization and quiescence monitoring |
| `runSequence` | `Svc.CmdSeqIn` | Output | Port for dispatching command sequences to the command sequencer |
| `completeSequence` | `Fw.CmdResponse` | Input (sync) | Receives completion status from the command sequencer after startup sequence execution |

## Component States

| State Variable | Type | Description |
|----------------|------|-------------|
| `m_boot_count` | `FwSizeType` | Current boot count. Zero indicates uninitialized state |
| `m_quiescence_start` | `Fw::Time` | Time when quiescence period started (mission epoch) |
| `m_waiting` | `std::atomic<bool>` | True when waiting for quiescence period to elapse |
| `m_stored_opcode` | `FwOpcodeType` | Opcode of pending `WAIT_FOR_QUIESCENCE` command |
| `m_stored_sequence` | `U32` | Sequence number of pending `WAIT_FOR_QUIESCENCE` command |

## Sequence Diagrams

## Parameters

| Name | Type | Default Value | Description |
|------|------|---------------|-------------|
| `ARMED` | `bool` | `true` | When true, system waits for quiescence period. When false, quiescence is bypassed |
| `QUIESCENCE_TIME` | `Fw.TimeIntervalValue` | `{seconds = 45 * 60, useconds = 0}` | Duration to wait for quiescence (45 minutes by default) |
| `QUIESCENCE_START_FILE` | `string` | `"/quiescence_start.bin"` | File path for storing the mission-wide quiescence start time |
| `STARTUP_SEQUENCE_FILE` | `string` | `"/startup.bin"` | Path to the command sequence file to run at startup |
| `BOOT_COUNT_FILE` | `string` | `"/boot_count.bin"` | File path for storing the boot count |

## Commands

| Name |  Description |
|------|-------------|
| `WAIT_FOR_QUIESCENCE` | Lets you start with opcode cmdseq and whether or not waiting |

## Events

| Name | Severity | Arguments | Description |
|------|----------|-----------|-------------|
| `BootCountUpdateFailure` | WARNING_LO | None | Emitted when the boot count file cannot be updated. Boot count was incremented in memory but not persisted |
| `QuiescenceFileInitFailure` | WARNING_LO | None | Emitted when the quiescence start time file cannot be initialized. System will use current time but cannot persist it |
| `StartupSequenceFinished` | ACTIVITY_LO | None | Emitted when the startup sequence completes successfully |
| `StartupSequenceFailed` | WARNING_LO | `response: Fw.CmdResponse` | Emitted when the startup sequence fails, includes the failure response code |

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


### Unit Tests


## Change Log

| Date | Author | Description |
|------|--------|-------------|

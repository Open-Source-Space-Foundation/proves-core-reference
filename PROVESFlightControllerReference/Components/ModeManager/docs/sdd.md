---
title: Mode Manager
category: Core
---

# Components::ModeManager

The ModeManager component manages system operational modes and orchestrates transitions between NORMAL and SAFE_MODE. It evaluates voltage conditions and detects unintended reboots to make mode decisions, controls power to non-critical subsystems during transitions, and maintains/persists mode state across reboots.

## Requirements
| Name | Description | Validation |
|---|---|---|
| MM0001 | The ModeManager shall maintain two operational modes: NORMAL and SAFE_MODE | Integration Testing |
| MM0002 | The ModeManager shall enter safe mode when commanded manually via FORCE_SAFE_MODE command | Integration Testing |
| MM0003 | The ModeManager shall enter safe mode when requested by external components via forceSafeMode port | Integration Testing |
| MM0004 | The ModeManager shall exit safe mode only via explicit EXIT_SAFE_MODE command or automatic voltage recovery | Integration Testing |
| MM0005 | The ModeManager shall turn off all 8 load switches when entering safe mode | Integration Testing |
| MM0006 | The ModeManager shall turn on face load switches (0-5) when exiting safe mode; payload switches (6-7) remain off | Integration Testing |
| MM0007 | The ModeManager shall persist mode state to non-volatile storage and restore on initialization | Integration Testing |
| MM0008 | The ModeManager shall detect unintended reboots and enter safe mode with reason SYSTEM_FAULT | Integration Testing |
| MM0009 | The ModeManager shall automatically enter safe mode when voltage drops below configurable threshold | Integration Testing |
| MM0010 | The ModeManager shall automatically exit safe mode (LOW_BATTERY only) when voltage recovers above configurable threshold | Integration Testing |
| MM0011 | The ModeManager shall enter safe mode with reason COMMAND_LOSS if no authenticated packet is received within COMM_LOSS_TIME; packets that only bypass authentication do not reset the timer | Integration Testing, Unit Testing |
| MM0012 | The ModeManager shall persist command loss elapsed time at least every 60 s and resume it after a reboot | Unit Testing |

## Class Diagram

```mermaid
classDiagram
    class ModeManager {
        <<Active Component>>
        - m_mode: SystemMode
        - m_safeModeEntryCount: U32
        - m_safeModeReason: SafeModeReason
        - m_safeModeVoltageCounter: U32
        - m_recoveryVoltageCounter: U32
        - m_commandLoss: CommandLossTimer
        + init(queueDepth, instance)
        - run_handler()
        - forceSafeMode_handler(reason)
        - getMode_handler(): SystemMode
        - prepareForReboot_handler()
        - authenticatedPacketRouted_handler()
        - enterSafeMode(reason)
        - exitSafeMode()
        - exitSafeModeAutomatic(voltage)
    }
    class SystemMode {
        <<enumeration>>
        SAFE_MODE = 1
        NORMAL = 2
    }
    class SafeModeReason {
        <<enumeration>>
        NONE = 0
        LOW_BATTERY = 1
        SYSTEM_FAULT = 2
        GROUND_COMMAND = 3
        EXTERNAL_REQUEST = 4
        LORA = 5
    }
    ModeManager --> SystemMode
    ModeManager --> SafeModeReason
```

## Ports

### Input Ports
| Name | Type | Kind | Description |
|---|---|---|---|
| run | Svc.Sched | sync | 1Hz periodic calls for telemetry, voltage monitoring, and command loss detection |
| forceSafeMode | ForceSafeModeWithReason | async | Safe mode requests from external components |
| getMode | GetSystemMode | sync | Query current system mode |
| prepareForReboot | Fw.Signal | sync | Set clean shutdown flag before intentional reboot |
| authenticatedPacketRouted | Fw.Signal | sync | Resets the command loss timer when ProvesRouter routes an authenticated packet (bypassed packets do not fire it) |

### Output Ports
| Name | Type | Description |
|---|---|---|
| modeChanged | SystemModeChanged | Notifies components of mode changes |
| loadSwitchTurnOn | Fw.Signal [8] | Turn on load switches |
| loadSwitchTurnOff | Fw.Signal [8] | Turn off load switches |
| voltageGet | Drv.VoltageGet | Query system voltage |
| stopWatchdog | Fw.Signal | Stops the hardware watchdog to trigger a power cycle (called on command loss) |

## Commands

| Name | Description |
|---|---|
| FORCE_SAFE_MODE | Forces safe mode with reason GROUND_COMMAND |
| EXIT_SAFE_MODE | Exits safe mode (fails if not in safe mode) |

## Parameters

Voltage thresholds and command loss timeout are configurable via F-Prime parameters:

| Parameter | Type | Default | Description |
|---|---|---|---|
| SafeModeEntryVoltage | F32 | 6.7 | Voltage (V) below which safe mode is entered |
| SafeModeRecoveryVoltage | F32 | 8.0 | Voltage (V) above which safe mode can be exited |
| SafeModeDebounceSeconds | U32 | 10 | Consecutive seconds required for transitions |
| COMM_LOSS_TIME | Fw.TimeIntervalValue | {seconds=3*60*60*24} | Time without an authenticated packet before command loss safe mode entry (default: 3 days) |

Parameters can be modified at runtime via `PRM_SET` commands.

## Events

| Name | Severity | Description |
|---|---|---|
| EnteringSafeMode | WARNING_HI | Entering safe mode with reason string |
| ExitingSafeMode | ACTIVITY_HI | Manually exiting safe mode |
| AutoSafeModeEntry | WARNING_HI | Auto-entry due to low voltage |
| AutoSafeModeExit | ACTIVITY_HI | Auto-exit due to voltage recovery |
| UnintendedRebootDetected | WARNING_HI | Unintended reboot detected on startup |
| ManualSafeModeEntry | ACTIVITY_HI | Safe mode commanded via FORCE_SAFE_MODE |
| ExternalFaultDetected | WARNING_HI | External component triggered safe mode |
| PreparingForReboot | ACTIVITY_HI | Clean shutdown flag being set |
| CommandValidationFailed | WARNING_LO | Command validation failed |
| StatePersistenceFailure | WARNING_LO | State save/load failed |
| CommandLossDetected | WARNING_HI | Command loss timeout exceeded; entering safe mode with reason COMMAND_LOSS |

## Telemetry

| Name | Type | Description |
|---|---|---|
| CurrentMode | U8 | Current mode (1=SAFE_MODE, 2=NORMAL) |
| SafeModeEntryCount | U32 | Times safe mode entered (persists across reboots) |
| CurrentSafeModeReason | SafeModeReason | Current reason (NONE if not in safe mode) |

## State Persistence

State is persisted to `/mode_state.bin`:
- Current mode (U8)
- Safe mode entry count (U32)
- Safe mode reason (U8)
- Clean shutdown flag (U8)

Command loss elapsed time is persisted separately to `/comm_loss_elapsed.bin` (8 bytes: U32 seconds little-endian, then its bitwise complement). See [Command Loss Detection](#command-loss-detection).

## Safe Mode Reason Logic

| Reason | Trigger | Auto-Recovery |
|---|---|---|
| LOW_BATTERY | Voltage below threshold | Yes (when voltage recovers) |
| SYSTEM_FAULT | Unintended reboot detected | No |
| GROUND_COMMAND | FORCE_SAFE_MODE command | No |
| EXTERNAL_REQUEST | forceSafeMode port call or command loss timeout | No |
| LORA | LoRa communication fault | No |

## Load Switch Mapping

| Index | Subsystem | NORMAL | SAFE_MODE |
|---|---|---|---|
| 0-5 | Satellite Faces | ON | OFF |
| 6-7 | Payload Power/Battery | OFF | OFF |

> When exiting to NORMAL, only face switches (0-5) turn ON. Payload switches must be controlled separately.

## Sequence Diagrams

### Command Loss Detection
```mermaid
sequenceDiagram
    participant AuthRouter as ProvesRouter
    participant ModeManager
    participant RateGroup

    participant Flash as /comm_loss_elapsed.bin

    Flash-->>ModeManager: restorePersistentState(): resume elapsed seconds
    AuthRouter->>ModeManager: authenticatedPacketRouted() [authenticated packets only]
    Note over ModeManager: elapsed = 0 in RAM (no file write here)

    loop Every 1Hz
        RateGroup->>ModeManager: run()
        ModeManager->>ModeManager: elapsed += 1
        opt First tick after a reset, or 60 s since the last write
            ModeManager->>Flash: write elapsed
        end
        ModeManager->>ModeManager: elapsed >= COMM_LOSS_TIME?
    end
    Note over ModeManager: Timeout exceeded
    ModeManager->>ModeManager: log CommandLossDetected event
    ModeManager->>ModeManager: enterSafeMode(COMMAND_LOSS)
    ModeManager->>ModeManager: restart elapsed at 0, latch until next authenticated packet
    ModeManager->>RateGroup: stopWatchdog (hardware power cycle)
```

The dead-man counts uptime seconds, not RTC time, so an RTC reset or time jump does not move it. `CommandLossTimer` (pure C++, unit tested in `test/unit-tests/test_ModeManager_CommandLossTimer.cpp`) owns counting, the write cadence, and resume; ModeManager only does the file I/O.

- **Resume:** on boot the stored value is loaded and counting continues from it, so a reboot loop cannot hold off command loss. A reboot loses at most the 60 s since the last write. A reboot loop shorter than 60 s never reaches a write and so does not advance the stored value.
- **Writes:** one write every 60 s without contact (1,440 per day), plus one write of 0 on the tick after the first authenticated packet. Further packets while the stored value is 0 cost nothing. Packets never write directly.
- **Missing or corrupt file:** start at 0 and emit `StatePersistenceFailure` (`cmdloss-load-open` or `cmdloss-load-corrupt`). A failed write emits `cmdloss-save-open` or `cmdloss-save-write`; the RAM timer keeps running.
- **After expiry:** the count restarts at 0 so the boot after the watchdog power cycle waits a full COMM_LOSS_TIME again instead of re-tripping.

### Safe Mode Entry (Low Voltage)
```mermaid
sequenceDiagram
    participant RateGroup
    participant ModeManager
    participant INA219
    participant LoadSwitches

    loop Every 1Hz
        RateGroup->>ModeManager: run()
        ModeManager->>INA219: voltageGet_out()
        INA219-->>ModeManager: voltage < threshold
        ModeManager->>ModeManager: Increment counter
    end
    Note over ModeManager: After debounce period
    ModeManager->>ModeManager: enterSafeMode(LOW_BATTERY)
    ModeManager->>LoadSwitches: Turn off all 8 switches
```

### Unintended Reboot Detection
```mermaid
sequenceDiagram
    participant Boot
    participant ModeManager
    participant FlashStorage

    Boot->>ModeManager: init()
    ModeManager->>FlashStorage: Load state
    FlashStorage-->>ModeManager: cleanShutdown=0, mode=NORMAL
    ModeManager->>ModeManager: Detect unintended reboot
    ModeManager->>ModeManager: enterSafeMode(SYSTEM_FAULT)
```

## Design Notes

- **Hysteresis**: Entry threshold (6.7V) < Recovery threshold (8.0V) prevents oscillation
- **Debounce**: Configurable consecutive samples prevent spurious transitions
- **Reason tracking**: Only LOW_BATTERY allows auto-recovery; other reasons require manual EXIT_SAFE_MODE
- **Mode query**: Both pull (getMode) and push (modeChanged) patterns supported
- **Command loss ownership**: ProvesRouter signals `authenticatedPacketRouted` on each routed authenticated packet; ModeManager owns the timer and the mode transition, keeping routing and mode management as separate concerns
- **Command loss thread safety**: `m_commandLoss` is protected by `m_commandLossMutex` since `authenticatedPacketRouted_handler` (called from the radio thread) and `run_handler` (called from the rate group thread) may run concurrently. The file write happens outside the lock

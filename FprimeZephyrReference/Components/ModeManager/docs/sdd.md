# Components::ModeManager

The ModeManager component manages system operational modes and orchestrates transitions across NORMAL, SAFE_MODE, and PAYLOAD_MODE. It monitors battery voltage, detects unintended reboots, and makes autonomous mode decisions to protect the satellite. It controls power to non‑critical subsystems during transitions and maintains/persists mode state across reboots to ensure consistent post‑recovery behavior.

Future work: a HIBERNATION mode remains planned; it will follow the same persistence and validation patterns once implemented.

## Requirements
| Name | Description | Validation |
|---|---|---|
| MM0001 | The ModeManager shall maintain three operational modes: NORMAL, SAFE_MODE, and PAYLOAD_MODE | Integration Testing |
| MM0002 | The ModeManager shall enter safe mode when commanded manually via FORCE_SAFE_MODE command | Integration Testing |
| MM0003 | The ModeManager shall enter safe mode when requested by external components via forceSafeMode port | Integration Testing |
| MM0004 | The ModeManager shall exit safe mode via EXIT_SAFE_MODE command or automatically when voltage recovers (LOW_BATTERY reason only) | Integration Testing |
| MM0005 | The ModeManager shall prevent exit from safe mode when not currently in safe mode | Integration Testing |
| MM0006 | The ModeManager shall turn off all 8 load switches when entering safe mode | Integration Testing |
| MM0007 | The ModeManager shall turn on face load switches (indices 0-5) when exiting safe mode; payload switches remain off until payload mode | Integration Testing |
| MM0008 | The ModeManager shall persist current mode, safe mode entry count, safe mode reason, and clean shutdown flag to non-volatile storage | Integration Testing |
| MM0009 | The ModeManager shall restore mode state from persistent storage on initialization | Integration Testing |
| MM0010 | The ModeManager shall track and report the number of times safe mode has been entered | Integration Testing |
| MM0011 | The ModeManager shall allow downstream components to query the current mode via getMode port | Unit Testing |
| MM0012 | The ModeManager shall notify downstream components of mode changes with the new mode value | Unit Testing |
| MM0013 | The ModeManager shall enter payload mode when commanded via ENTER_PAYLOAD_MODE while in NORMAL and reject entry from SAFE_MODE | Integration Testing |
| MM0014 | The ModeManager shall exit payload mode only via explicit EXIT_PAYLOAD_MODE command and reject exit when not in payload mode | Integration Testing |
| MM0016 | The ModeManager shall turn on all load switches (faces 0-5 and payload 6-7) when entering payload mode and turn off payload load switches when exiting payload mode | Integration Testing |
| MM0017 | The ModeManager shall track and report the number of times payload mode has been entered | Integration Testing |
| MM0018 | The ModeManager shall persist payload mode state and payload mode entry count to non-volatile storage and restore them on initialization | Integration Testing |
| MM0019 | The ModeManager shall reject FORCE_SAFE_MODE from payload mode (must exit payload mode first for sequential transitions) | Integration Testing |
| MM0020 | While in payload mode, the ModeManager shall monitor bus voltage once per second and automatically exit payload mode after 10 consecutive seconds below 7.2V or upon invalid voltage readings | Integration Testing (manual + debounced behavior) |
| MM0021 | Automatic payload mode exit shall emit AutoPayloadModeExit and turn off all 8 load switches | Integration Testing (manual) |
| MM0022 | While in NORMAL mode, the ModeManager shall automatically enter safe mode after 10 consecutive seconds with voltage below 6.7V (reason: LOW_BATTERY) | Integration Testing (manual) |
| MM0023 | While in SAFE_MODE with reason LOW_BATTERY, the ModeManager shall automatically exit to NORMAL after 10 consecutive seconds with voltage above 8.0V | Integration Testing (manual) |
| MM0024 | SAFE_MODE with reason SYSTEM_FAULT or GROUND_COMMAND shall NOT auto-recover; manual EXIT_SAFE_MODE is required | Integration Testing |
| MM0025 | The ModeManager shall detect unintended reboots via clean shutdown flag and enter SAFE_MODE with reason SYSTEM_FAULT | Integration Testing (manual) |
| MM0026 | The ModeManager shall set clean shutdown flag when notified via prepareForReboot port before intentional reboot | Integration Testing |
| MM0027 | The ModeManager shall track and report the current safe mode reason via telemetry | Integration Testing |
| MM0028 | The ModeManager shall log when forceSafeMode request is ignored while in PAYLOAD_MODE | Integration Testing |

## Usage Examples

The ModeManager component operates as an active component that manages system-wide operational modes. It runs at 1Hz via the rate group and responds to commands and external fault conditions.

### Typical Usage

1. **System Initialization**
   - Component is instantiated during system startup
   - Loads previous mode state from `/mode_state.bin`
   - Configures load switches to match restored mode
   - Begins 1Hz periodic execution via rate group

2. **Normal Operation**
   - Updates telemetry channels (CurrentMode, SafeModeEntryCount, PayloadModeEntryCount)
   - Responds to mode query requests from downstream components
   - Keeps payload load switches (indices 6 and 7) off unless payload mode is explicitly entered

3. **Safe Mode Entry**
   - Can be triggered by (only from NORMAL mode - sequential transitions enforced):
     - Ground command: `FORCE_SAFE_MODE`
     - External component request via `forceSafeMode` port
   - Actions performed:
     - Transitions mode to SAFE_MODE
     - Increments safe mode entry counter
     - Emits `EnteringSafeMode` event with reason
     - Turns off all 8 load switches
     - Notifies downstream components via `modeChanged` port
     - Persists state to flash storage

4. **Payload Mode Entry**
   - Triggered by ground command: `ENTER_PAYLOAD_MODE` (only allowed from NORMAL)
   - Actions performed:
     - Transitions mode to PAYLOAD_MODE
     - Increments payload mode entry counter
     - Emits `EnteringPayloadMode` event and, for commands, `ManualPayloadModeEntry`
     - Turns on **all load switches** (faces 0-5 and payload 6-7) to ensure consistent power state even after fault exits
     - Notifies downstream components via `modeChanged` port
     - Updates telemetry (CurrentMode, PayloadModeEntryCount)
     - Persists state to flash storage

5. **Payload Mode Exit**
   - Triggered by ground command: `EXIT_PAYLOAD_MODE`
   - Validates currently in payload mode before allowing exit
   - Actions performed:
     - Transitions mode to NORMAL
     - Emits `ExitingPayloadMode` event
     - Turns off payload load switches (indices 6 and 7)
     - Ensures face load switches (0-5) are turned ON for NORMAL mode
     - Notifies downstream components via `modeChanged` port
     - Updates telemetry
     - Persists state to flash storage

6. **Automatic Payload Mode Exit (Low Voltage)**
   - Triggered when in PAYLOAD_MODE and bus voltage is below 7.2V (or invalid) for 10 consecutive 1Hz checks
   - Actions performed:
     - Emits `AutoPayloadModeExit` with measured/0.0V value
     - Transitions mode to NORMAL
     - Turns off **all 8 load switches** (faces and payload) aggressively
     - Notifies downstream components via `modeChanged` port
     - Updates telemetry and persists state

7. **Safe Mode Exit (Manual)**
   - Triggered by ground command: `EXIT_SAFE_MODE`
   - Validates currently in safe mode before allowing exit
   - Actions performed:
     - Transitions mode to NORMAL
     - Clears safe mode reason to NONE
     - Emits `ExitingSafeMode` event
     - Turns on face load switches (indices 0-5); payload switches remain off until explicitly entering payload mode
     - Notifies downstream components via `modeChanged` port
     - Persists state to flash storage

8. **Automatic Safe Mode Entry (Low Voltage)**
   - Triggered when in NORMAL mode and bus voltage is below 6.7V for 10 consecutive 1Hz checks
   - Actions performed:
     - Emits `AutoSafeModeEntry` with measured voltage value
     - Transitions mode to SAFE_MODE with reason LOW_BATTERY
     - Increments safe mode entry counter
     - Emits `EnteringSafeMode` event with reason "Low battery voltage"
     - Turns off all 8 load switches
     - Notifies downstream components via `modeChanged` port
     - Persists state to flash storage

9. **Automatic Safe Mode Exit (Voltage Recovery)**
   - Triggered when in SAFE_MODE with reason LOW_BATTERY and bus voltage is above 8.0V for 10 consecutive 1Hz checks
   - Does NOT apply to SYSTEM_FAULT, GROUND_COMMAND, or EXTERNAL_REQUEST reasons (manual exit required)
   - Actions performed:
     - Emits `AutoSafeModeExit` with measured voltage value
     - Transitions mode to NORMAL
     - Clears safe mode reason to NONE
     - Emits `ExitingSafeMode` event
     - Turns on face load switches (indices 0-5)
     - Notifies downstream components via `modeChanged` port
     - Persists state to flash storage

10. **Unintended Reboot Detection**
    - On initialization, checks clean shutdown flag from persistent state
    - If flag is false (unclean shutdown), system enters SAFE_MODE with reason SYSTEM_FAULT
    - Actions performed:
      - Emits `UnintendedRebootDetected` event
      - Enters safe mode via `enterSafeMode(SYSTEM_FAULT)`
    - The clean shutdown flag is set via `prepareForReboot` port before intentional reboots

11. **Clean Shutdown Handling**
    - When ResetManager is about to perform intentional reboot, it calls `prepareForReboot` port
    - Actions performed:
      - Emits `PreparingForReboot` event
      - Sets clean shutdown flag to true
      - Persists state to flash storage
    - This prevents false unintended reboot detection on next boot

12. **Mode Queries**
   - Downstream components can call `getMode` port to query current mode
   - Returns immediate synchronous response with current mode

## Class Diagram

```mermaid
classDiagram
    namespace Components {
        class ModeManagerComponentBase {
            <<Auto-generated>>
        }
        class ModeManager {
            <<Active Component>>
            - m_mode: SystemMode
            - m_safeModeReason: SafeModeReason
            - m_safeModeEntryCount: U32
            - m_payloadModeEntryCount: U32
            - m_runCounter: U32
            - m_cleanShutdownFlag: bool
            - STATE_FILE_PATH: const char*
            + ModeManager(const char* compName)
            + ~ModeManager()
            + init(FwSizeType queueDepth, FwEnumStoreType instance)
            - run_handler(FwIndexType portNum, U32 context)
            - forceSafeMode_handler(FwIndexType portNum)
            - prepareForReboot_handler(FwIndexType portNum)
            - getMode_handler(FwIndexType portNum): SystemMode
            - FORCE_SAFE_MODE_cmdHandler(FwOpcodeType opCode, U32 cmdSeq)
            - EXIT_SAFE_MODE_cmdHandler(FwOpcodeType opCode, U32 cmdSeq)
            - ENTER_PAYLOAD_MODE_cmdHandler(FwOpcodeType opCode, U32 cmdSeq)
            - EXIT_PAYLOAD_MODE_cmdHandler(FwOpcodeType opCode, U32 cmdSeq)
            - loadState()
            - saveState()
            - enterSafeMode(SafeModeReason reason)
            - exitSafeMode()
            - enterPayloadMode(const char* reason)
            - exitPayloadMode()
            - exitPayloadModeAutomatic(F32 voltage)
            - turnOffNonCriticalComponents()
            - turnOnComponents()
            - turnOnPayload()
            - turnOffPayload()
            - getCurrentVoltage(bool& valid): F32
            - m_lowVoltageCounter: U32
            - m_highVoltageCounter: U32
            - PAYLOAD_LOW_VOLTAGE_THRESHOLD: F32 = 7.2V
            - SAFE_MODE_VOLTAGE_THRESHOLD: F32 = 6.7V
            - SAFE_MODE_RECOVERY_VOLTAGE_THRESHOLD: F32 = 8.0V
            - LOW_VOLTAGE_DEBOUNCE_SECONDS: U32 = 10
        }
        class SystemMode {
            <<enumeration>>
            SAFE_MODE = 1
            NORMAL = 2
            PAYLOAD_MODE = 3
        }
        class SafeModeReason {
            <<enumeration>>
            NONE = 0
            LOW_BATTERY = 1
            SYSTEM_FAULT = 2
            GROUND_COMMAND = 3
            EXTERNAL_REQUEST = 4
        }
    }
    ModeManagerComponentBase <|-- ModeManager : inherits
    ModeManager --> SystemMode : uses
    ModeManager --> SafeModeReason : uses
```

## Port Descriptions

### Input Ports
| Name | Type | Kind | Description |
|---|---|---|---|
| run | Svc.Sched | sync | Receives periodic calls from rate group (1Hz) for telemetry updates, low-voltage monitoring in PAYLOAD_MODE, and auto-recovery monitoring in SAFE_MODE |
| forceSafeMode | Fw.Signal | async | Receives safe mode requests from external components detecting faults (only acts when in NORMAL mode; logs warning in PAYLOAD_MODE) |
| prepareForReboot | Fw.Signal | async | Called by ResetManager before intentional reboot; sets clean shutdown flag to prevent false unintended reboot detection |
| getMode | Components.GetSystemMode | sync | Allows downstream components to query current system mode |

### Output Ports
| Name | Type | Kind | Description |
|---|---|---|---|
| modeChanged | Components.SystemModeChanged | output | Notifies downstream components of mode changes with new mode value |
| loadSwitchTurnOn | Fw.Signal [8] | output | Signals to turn on each of 8 load switches (faces 0-5, payload power, payload battery) |
| loadSwitchTurnOff | Fw.Signal [8] | output | Signals to turn off each of 8 load switches |
| voltageGet | Drv.VoltageGet | output | Queries INA219 manager for current system voltage |

## Component States

| Name | Type | Description |
|---|---|---|
| m_mode | SystemMode | Current operational mode (NORMAL, SAFE_MODE, or PAYLOAD_MODE) |
| m_safeModeReason | SafeModeReason | Reason for current safe mode entry (NONE, LOW_BATTERY, SYSTEM_FAULT, GROUND_COMMAND, EXTERNAL_REQUEST) |
| m_safeModeEntryCount | U32 | Number of times safe mode has been entered since initial deployment |
| m_payloadModeEntryCount | U32 | Number of times payload mode has been entered since initial deployment |
| m_runCounter | U32 | Counter for 1Hz run handler calls |
| m_lowVoltageCounter | U32 | Debounce counter for consecutive low/invalid voltage readings (used in PAYLOAD_MODE for auto-exit and NORMAL mode for auto-safe-mode-entry) |
| m_highVoltageCounter | U32 | Debounce counter for consecutive high voltage readings (used in SAFE_MODE for auto-recovery when reason is LOW_BATTERY) |
| m_cleanShutdownFlag | bool | Flag indicating if last shutdown was clean (set via prepareForReboot port); used for unintended reboot detection |

### Persistent State
The component persists the following state to `/mode_state.bin`:
- Current mode (U8)
- Safe mode entry count (U32)
- Payload mode entry count (U32)
- Safe mode reason (U8)
- Clean shutdown flag (U8)

This state is loaded on initialization and saved on every mode transition and before intentional reboots.

## Sequence Diagrams

### Safe Mode Entry (Command)
```mermaid
sequenceDiagram
    participant Ground
    participant ModeManager
    participant LoadSwitches
    participant DownstreamComponents
    participant FlashStorage

    Ground->>ModeManager: FORCE_SAFE_MODE command
    ModeManager->>ModeManager: Emit ManualSafeModeEntry event
    ModeManager->>ModeManager: Set m_mode = SAFE_MODE
    ModeManager->>ModeManager: Increment m_safeModeEntryCount
    ModeManager->>ModeManager: Emit EnteringSafeMode event
    ModeManager->>LoadSwitches: Turn off all 8 switches
    ModeManager->>ModeManager: Update telemetry
    ModeManager->>DownstreamComponents: modeChanged_out(SAFE_MODE)
    ModeManager->>FlashStorage: Save state to /mode_state.bin
    ModeManager->>Ground: Command response OK
```

### Safe Mode Entry (External Request)
```mermaid
sequenceDiagram
    participant ExternalComponent
    participant ModeManager
    participant LoadSwitches
    participant DownstreamComponents
    participant FlashStorage

    ExternalComponent->>ModeManager: forceSafeMode port call
    ModeManager->>ModeManager: Emit ExternalFaultDetected event
    ModeManager->>ModeManager: Set m_mode = SAFE_MODE
    ModeManager->>ModeManager: Increment m_safeModeEntryCount
    ModeManager->>ModeManager: Emit EnteringSafeMode event
    ModeManager->>LoadSwitches: Turn off all 8 switches
    ModeManager->>ModeManager: Update telemetry
    ModeManager->>DownstreamComponents: modeChanged_out(SAFE_MODE)
    ModeManager->>FlashStorage: Save state to /mode_state.bin
```

### Safe Mode Exit
```mermaid
sequenceDiagram
    participant Ground
    participant ModeManager
    participant LoadSwitches
    participant DownstreamComponents
    participant FlashStorage

    Ground->>ModeManager: EXIT_SAFE_MODE command
    ModeManager->>ModeManager: Validate currently in SAFE_MODE
    ModeManager->>ModeManager: Set m_mode = NORMAL
    ModeManager->>ModeManager: Emit ExitingSafeMode event
    ModeManager->>LoadSwitches: Turn on face switches (indices 0-5)
    ModeManager->>ModeManager: Update telemetry
    ModeManager->>DownstreamComponents: modeChanged_out(NORMAL)
    ModeManager->>FlashStorage: Save state to /mode_state.bin
    ModeManager->>Ground: Command response OK
```

### Payload Mode Entry (Command)
```mermaid
sequenceDiagram
    participant Ground
    participant ModeManager
    participant LoadSwitches
    participant DownstreamComponents
    participant FlashStorage

    Ground->>ModeManager: ENTER_PAYLOAD_MODE command
    ModeManager->>ModeManager: Validate currently in NORMAL
    ModeManager->>ModeManager: Emit ManualPayloadModeEntry event
    ModeManager->>ModeManager: Set m_mode = PAYLOAD_MODE
    ModeManager->>ModeManager: Increment m_payloadModeEntryCount
    ModeManager->>ModeManager: Emit EnteringPayloadMode event
    ModeManager->>LoadSwitches: Turn on all load switches (faces 0-5 and payload 6 & 7)
    ModeManager->>ModeManager: Update telemetry
    ModeManager->>DownstreamComponents: modeChanged_out(PAYLOAD_MODE)
    ModeManager->>FlashStorage: Save state to /mode_state.bin
    ModeManager->>Ground: Command response OK
```

### Payload Mode Exit (Command)
```mermaid
sequenceDiagram
    participant Ground
    participant ModeManager
    participant LoadSwitches
    participant DownstreamComponents
    participant FlashStorage

    Ground->>ModeManager: EXIT_PAYLOAD_MODE command
    ModeManager->>ModeManager: Validate currently in PAYLOAD_MODE
    ModeManager->>ModeManager: Set m_mode = NORMAL
    ModeManager->>ModeManager: Emit ExitingPayloadMode event
    ModeManager->>LoadSwitches: Turn off payload switches (6 & 7)
    ModeManager->>LoadSwitches: Turn on face switches (0-5) to normalize NORMAL state
    ModeManager->>ModeManager: Update telemetry
    ModeManager->>DownstreamComponents: modeChanged_out(NORMAL)
    ModeManager->>FlashStorage: Save state to /mode_state.bin
    ModeManager->>Ground: Command response OK
```

### Automatic Payload Mode Exit (Low Voltage)
```mermaid
sequenceDiagram
    participant ModeManager
    participant INA219
    participant LoadSwitches
    participant DownstreamComponents
    participant FlashStorage

    ModeManager->>INA219: voltageGet()
    INA219-->>ModeManager: Voltage / invalid
    ModeManager->>ModeManager: Debounce low-voltage/invalid readings (10 consecutive seconds)
    ModeManager->>ModeManager: On debounce hit: Emit AutoPayloadModeExit(voltage)
    ModeManager->>ModeManager: Set m_mode = NORMAL
    ModeManager->>LoadSwitches: Turn off all 8 switches (faces and payload)
    ModeManager->>ModeManager: Update telemetry
    ModeManager->>DownstreamComponents: modeChanged_out(NORMAL)
    ModeManager->>FlashStorage: Save state to /mode_state.bin
```

### Mode Query
```mermaid
sequenceDiagram
    participant DownstreamComponent
    participant ModeManager

    DownstreamComponent->>ModeManager: getMode() port call
    ModeManager->>ModeManager: Read m_mode
    ModeManager-->>DownstreamComponent: Return current mode (NORMAL, SAFE_MODE, or PAYLOAD_MODE)
```

### Periodic Execution (1Hz)
```mermaid
sequenceDiagram
    participant RateGroup
    participant ModeManager
    participant INA219

    RateGroup->>ModeManager: run(portNum, context)
    ModeManager->>ModeManager: Increment m_runCounter
    alt PAYLOAD_MODE
        ModeManager->>INA219: voltageGet()
        INA219-->>ModeManager: voltage / invalid
        ModeManager->>ModeManager: Debounce low-voltage (<7.2V) counter
        ModeManager->>ModeManager: If 10 consecutive: exitPayloadModeAutomatic()
    else NORMAL
        ModeManager->>INA219: voltageGet()
        INA219-->>ModeManager: voltage / invalid
        ModeManager->>ModeManager: Debounce low-voltage (<6.7V) counter
        ModeManager->>ModeManager: If 10 consecutive: enterSafeMode(LOW_BATTERY)
    else SAFE_MODE with reason LOW_BATTERY
        ModeManager->>INA219: voltageGet()
        INA219-->>ModeManager: voltage / invalid
        ModeManager->>ModeManager: Debounce high-voltage (>8.0V) counter
        ModeManager->>ModeManager: If 10 consecutive: exitSafeMode() auto-recovery
    end
    ModeManager->>ModeManager: Write CurrentMode telemetry
```

### Automatic Safe Mode Entry (Low Voltage)
```mermaid
sequenceDiagram
    participant ModeManager
    participant INA219
    participant LoadSwitches
    participant DownstreamComponents
    participant FlashStorage

    Note over ModeManager: In NORMAL mode, voltage <6.7V for 10s
    ModeManager->>INA219: voltageGet()
    INA219-->>ModeManager: Low voltage (e.g., 6.5V)
    ModeManager->>ModeManager: Increment m_lowVoltageCounter
    Note over ModeManager: After 10 consecutive low readings...
    ModeManager->>ModeManager: Emit AutoSafeModeEntry(voltage)
    ModeManager->>ModeManager: Set m_mode = SAFE_MODE
    ModeManager->>ModeManager: Set m_safeModeReason = LOW_BATTERY
    ModeManager->>ModeManager: Increment m_safeModeEntryCount
    ModeManager->>ModeManager: Emit EnteringSafeMode("Low battery voltage")
    ModeManager->>LoadSwitches: Turn off all 8 switches
    ModeManager->>DownstreamComponents: modeChanged_out(SAFE_MODE)
    ModeManager->>FlashStorage: Save state to /mode_state.bin
```

### Automatic Safe Mode Exit (Voltage Recovery)
```mermaid
sequenceDiagram
    participant ModeManager
    participant INA219
    participant LoadSwitches
    participant DownstreamComponents
    participant FlashStorage

    Note over ModeManager: In SAFE_MODE with reason LOW_BATTERY, voltage >8.0V for 10s
    ModeManager->>INA219: voltageGet()
    INA219-->>ModeManager: High voltage (e.g., 8.2V)
    ModeManager->>ModeManager: Increment m_highVoltageCounter
    Note over ModeManager: After 10 consecutive high readings...
    ModeManager->>ModeManager: Emit AutoSafeModeExit(voltage)
    ModeManager->>ModeManager: Set m_mode = NORMAL
    ModeManager->>ModeManager: Set m_safeModeReason = NONE
    ModeManager->>ModeManager: Emit ExitingSafeMode
    ModeManager->>LoadSwitches: Turn on face switches (0-5)
    ModeManager->>DownstreamComponents: modeChanged_out(NORMAL)
    ModeManager->>FlashStorage: Save state to /mode_state.bin
```

### Unintended Reboot Detection
```mermaid
sequenceDiagram
    participant System
    participant ModeManager
    participant FlashStorage
    participant LoadSwitches
    participant DownstreamComponents

    System->>ModeManager: init()
    ModeManager->>FlashStorage: loadState()
    FlashStorage-->>ModeManager: State with cleanShutdownFlag = false
    ModeManager->>ModeManager: Detect unintended reboot
    ModeManager->>ModeManager: Emit UnintendedRebootDetected
    ModeManager->>ModeManager: enterSafeMode(SYSTEM_FAULT)
    ModeManager->>ModeManager: Emit EnteringSafeMode("Unintended reboot detected")
    ModeManager->>LoadSwitches: Turn off all 8 switches
    ModeManager->>DownstreamComponents: modeChanged_out(SAFE_MODE)
    ModeManager->>FlashStorage: Save state (cleanShutdownFlag = false)
```

### Prepare for Reboot (Clean Shutdown)
```mermaid
sequenceDiagram
    participant ResetManager
    participant ModeManager
    participant FlashStorage

    ResetManager->>ModeManager: prepareForReboot port call
    ModeManager->>ModeManager: Emit PreparingForReboot
    ModeManager->>ModeManager: Set m_cleanShutdownFlag = true
    ModeManager->>FlashStorage: Save state with cleanShutdownFlag = true
    ResetManager->>ResetManager: sys_reboot()
```

## Commands

| Name | Arguments | Description |
|---|---|---|
| FORCE_SAFE_MODE | None | Forces the system into safe mode. Only allowed from NORMAL mode (rejects from PAYLOAD_MODE with validation error). Emits ManualSafeModeEntry event. Idempotent when already in safe mode. |
| EXIT_SAFE_MODE | None | Exits safe mode and returns to normal operation. Fails with CommandValidationFailed if not currently in safe mode. |
| ENTER_PAYLOAD_MODE | None | Enters payload mode from NORMAL. Fails with CommandValidationFailed if issued from SAFE_MODE or if already in payload mode (idempotent success when already in payload). Emits ManualPayloadModeEntry event. |
| EXIT_PAYLOAD_MODE | None | Exits payload mode and returns to normal operation. Fails with CommandValidationFailed if not currently in payload mode. |

## Events

| Name | Severity | Arguments | Description |
|---|---|---|---|
| EnteringSafeMode | WARNING_HI | reason: string size 100 | Emitted when entering safe mode, includes reason (e.g., "Ground command", "Low battery voltage") |
| ExitingSafeMode | ACTIVITY_HI | None | Emitted when exiting safe mode and returning to normal operation |
| ManualSafeModeEntry | ACTIVITY_HI | None | Emitted when safe mode is manually commanded via FORCE_SAFE_MODE |
| ExternalFaultDetected | WARNING_HI | None | Emitted when an external component triggers safe mode via forceSafeMode port |
| AutoSafeModeEntry | WARNING_HI | voltage: F32 | Emitted when automatically entering safe mode due to low voltage (<6.7V for 10s) |
| AutoSafeModeExit | ACTIVITY_HI | voltage: F32 | Emitted when automatically exiting safe mode due to voltage recovery (>8.0V for 10s, only for LOW_BATTERY reason) |
| UnintendedRebootDetected | WARNING_HI | None | Emitted on initialization when clean shutdown flag is false, indicating an unexpected reboot |
| PreparingForReboot | ACTIVITY_HI | None | Emitted when prepareForReboot port is called before an intentional reboot |
| ExternalFaultIgnoredInPayloadMode | WARNING_LO | None | Emitted when forceSafeMode request is ignored because system is in PAYLOAD_MODE (must exit to NORMAL first) |
| EnteringPayloadMode | ACTIVITY_HI | reason: string size 100 | Emitted when entering payload mode, includes reason (e.g., "Ground command") |
| ExitingPayloadMode | ACTIVITY_HI | None | Emitted when exiting payload mode and returning to normal operation |
| ManualPayloadModeEntry | ACTIVITY_HI | None | Emitted when payload mode is manually commanded via ENTER_PAYLOAD_MODE |
| AutoPayloadModeExit | WARNING_HI | voltage: F32 | Emitted when automatically exiting payload mode due to low/invalid voltage (voltage value reported; 0.0 if invalid) |
| CommandValidationFailed | WARNING_LO | cmdName: string size 50<br>reason: string size 100 | Emitted when a command fails validation (e.g., EXIT_SAFE_MODE when not in safe mode) |
| StatePersistenceFailure | WARNING_LO | operation: string size 20<br>status: I32 | Emitted when state save/load operations fail (operations: save-open, save-write, load-open, load-read, load-corrupt, shutdown-open, shutdown-write) |

## Telemetry

| Name | Type | Update Rate | Description |
|---|---|---|---|
| CurrentMode | U8 | 1Hz | Current system mode (1 = SAFE_MODE, 2 = NORMAL, 3 = PAYLOAD_MODE) |
| CurrentSafeModeReason | U8 | On change | Current safe mode reason (0 = NONE, 1 = LOW_BATTERY, 2 = SYSTEM_FAULT, 3 = GROUND_COMMAND, 4 = EXTERNAL_REQUEST). Only valid when in SAFE_MODE. |
| SafeModeEntryCount | U32 | On change | Number of times safe mode has been entered (persists across reboots) |
| PayloadModeEntryCount | U32 | On change | Number of times payload mode has been entered (persists across reboots) |

## Load Switch Mapping

The ModeManager controls 8 load switches that power non-critical satellite subsystems:

| Index | Subsystem | NORMAL State | SAFE_MODE State | PAYLOAD_MODE State |
|---|---|---|---|---|
| 0 | Satellite Face 0 | ON | OFF | ON |
| 1 | Satellite Face 1 | ON | OFF | ON |
| 2 | Satellite Face 2 | ON | OFF | ON |
| 3 | Satellite Face 3 | ON | OFF | ON |
| 4 | Satellite Face 4 | ON | OFF | ON |
| 5 | Satellite Face 5 | ON | OFF | ON |
| 6 | Payload Power | OFF | OFF | ON |
| 7 | Payload Battery | OFF | OFF | ON |

> **Notes:** PAYLOAD_MODE can only be entered from NORMAL mode (not from SAFE_MODE). When restoring PAYLOAD_MODE from persistent storage after a reboot, both face switches (0-5) and payload switches (6-7) are explicitly turned ON to ensure consistent state. Automatic payload exits (low/invalid voltage) aggressively turn **all** switches OFF; manual payload exits leave faces ON and only shed payload loads.
## Integration Tests

See `FprimeZephyrReference/test/int/mode_manager_test.py`, `FprimeZephyrReference/test/int/payload_mode_test.py`, and `FprimeZephyrReference/test/int/safe_mode_test.py` for comprehensive integration tests covering:

| Test | Description | Coverage |
|---|---|---|
| **mode_manager_test.py** | | |
| test_01_initial_telemetry | Verifies initial telemetry can be read | Basic functionality |
| test_04_force_safe_mode_command | Tests FORCE_SAFE_MODE command enters safe mode | Safe mode entry |
| test_05_safe_mode_increments_counter | Verifies SafeModeEntryCount increments | Counter tracking |
| test_06_safe_mode_turns_off_load_switches | Verifies all load switches turn off in safe mode | Power management |
| test_07_safe_mode_emits_event | Verifies EnteringSafeMode event with correct reason | Event emission |
| test_13_exit_safe_mode_fails_not_in_safe_mode | Verifies EXIT_SAFE_MODE fails when not in safe mode | Command validation |
| test_14_exit_safe_mode_success | Tests successful safe mode exit | Safe mode exit |
| test_18_force_safe_mode_idempotent | Verifies FORCE_SAFE_MODE is idempotent when already in safe mode | Idempotency |
| test_19_safe_mode_state_persists | Verifies safe mode persistence to flash | State persistence |
| **payload_mode_test.py** | | |
| test_payload_01_enter_exit_payload_mode | Validates payload mode entry/exit, events, telemetry, payload load switches | Payload mode entry/exit |
| test_payload_02_cannot_enter_from_safe_mode | Ensures ENTER_PAYLOAD_MODE fails from SAFE_MODE | Command validation |
| test_payload_03_safe_mode_rejected_from_payload | Ensures FORCE_SAFE_MODE is rejected from payload mode (sequential transitions) | Command validation |
| test_payload_04_state_persists | Verifies payload mode and counters persist | Payload persistence |
| test_payload_05_manual_exit_face_switches_remain_on | Verifies manual payload exit leaves faces ON and payload OFF | Payload exit power behavior |
| test_payload_06_voltage_monitoring_active | Verifies voltage telemetry is present in payload mode and no false auto-exit when voltage healthy | Low-voltage monitoring sanity |
| test_payload_07_auto_exit_low_voltage (manual) | Manual test to validate debounced low-voltage auto-exit, AutoPayloadModeExit event, and full load-shed | Low-voltage protection |
| **safe_mode_test.py** | | |
| test_safe_01_auto_entry_low_voltage (manual) | Tests automatic safe mode entry when voltage drops below 6.7V for 10s | Auto safe mode entry |
| test_safe_02_auto_exit_voltage_recovery (manual) | Tests automatic safe mode exit when voltage recovers above 8.0V for 10s (LOW_BATTERY only) | Auto safe mode exit |
| test_safe_03_no_auto_exit_system_fault (manual) | Verifies no auto-exit for SYSTEM_FAULT reason | Manual exit required |
| test_safe_04_no_auto_exit_ground_command (manual) | Verifies no auto-exit for GROUND_COMMAND reason | Manual exit required |
| test_safe_05_unintended_reboot_detection (manual) | Tests safe mode entry on unintended reboot detection | Reboot detection |
| test_safe_06_clean_shutdown_flag (manual) | Verifies clean shutdown flag prevents false reboot detection | Clean shutdown |
| test_safe_07_safe_mode_reason_telemetry | Verifies CurrentSafeModeReason telemetry channel | Reason tracking |
| test_safe_08_external_fault_ignored_in_payload | Verifies forceSafeMode logs warning when ignored in PAYLOAD_MODE | Sequential transitions |
| test_safe_09_cascade_payload_to_safe_mode (manual) | Tests cascade PAYLOAD_MODE → NORMAL → SAFE_MODE on very low voltage | Cascade transitions |

## Design Decisions

### Hybrid Mode Query/Notification Approach
The component provides both pull-based (getMode port) and push-based (modeChanged port) mechanisms for mode awareness:

- **getMode port (pull)**: Allows components to query current mode on-demand, useful for:
  - Component initialization
  - Recovery from faults
  - Periodic mode checks

- **modeChanged port (push)**: Notifies components immediately when mode changes with the new mode value, useful for:
  - Reactive behavior
  - Real-time mode tracking
  - Avoiding polling overhead

This dual approach ensures downstream components can reliably track system mode even if they miss a transition notification.

### State Persistence
Mode state is persisted to `/mode_state.bin` to maintain operational context across:
- Intentional reboots
- Watchdog resets
- Power cycles

This ensures the system resumes in the correct mode after recovery.

### Low-Voltage Payload Protection
Payload mode is protected by a debounced low-voltage monitor:
- Voltage sampled at 1Hz while in PAYLOAD_MODE via `voltageGet`
- Threshold: 7.2V; invalid readings are treated as faults to avoid masking brownouts
- Debounce: 10 consecutive low/invalid readings before acting
- Action: emit `AutoPayloadModeExit`, set mode to NORMAL, and turn off all 8 load switches
- Re-entering payload mode re-powers faces and payload switches to restore a consistent state

### Sequential Mode Transitions
Mode transitions follow a +1/-1 sequential pattern: SAFE_MODE(1) ↔ NORMAL(2) ↔ PAYLOAD_MODE(3). Direct jumps (e.g., PAYLOAD→SAFE) are not allowed - users must exit payload mode first before entering safe mode. FORCE_SAFE_MODE is idempotent when already in safe mode. When forceSafeMode is called in PAYLOAD_MODE, it logs a warning (`ExternalFaultIgnoredInPayloadMode`) to inform operators that manual exit is required first.

### Automatic Safe Mode Entry/Exit
The component monitors voltage to automatically manage safe mode transitions:
- **Entry**: When in NORMAL mode and voltage drops below 6.7V for 10 consecutive seconds, the system automatically enters safe mode with reason LOW_BATTERY.
- **Exit (Auto-Recovery)**: When in SAFE_MODE with reason LOW_BATTERY and voltage rises above 8.0V for 10 consecutive seconds, the system automatically exits to NORMAL mode.
- **No Auto-Recovery**: Safe mode entries caused by SYSTEM_FAULT, GROUND_COMMAND, or EXTERNAL_REQUEST require manual EXIT_SAFE_MODE command. This ensures operator awareness after critical faults.

The hysteresis gap (6.7V entry vs 8.0V exit) prevents oscillation between modes.

### Unintended Reboot Detection
The component uses a clean shutdown flag to detect unexpected reboots:
- Before intentional reboots, ResetManager calls the `prepareForReboot` port, which sets the clean shutdown flag to true.
- On initialization, if the flag is false, the system detected an unintended reboot (watchdog reset, power glitch, crash) and enters safe mode with reason SYSTEM_FAULT.
- This ensures the satellite enters a known-safe state after any unexpected restart.

### Safe Mode Reason Tracking
Each safe mode entry records its reason, enabling different recovery behaviors:
- **LOW_BATTERY**: Automatic recovery possible when voltage recovers
- **SYSTEM_FAULT**: Manual intervention required (reboot, hardware issue)
- **GROUND_COMMAND**: Manual intervention required (operator-initiated)
- **EXTERNAL_REQUEST**: Manual intervention required (component fault)

## Change Log
| Date | Description |
|---|---|
| 2025-11-29 | Added automatic safe mode entry/exit based on voltage (6.7V/8.0V thresholds), unintended reboot detection via clean shutdown flag, SafeModeReason enum (NONE, LOW_BATTERY, SYSTEM_FAULT, GROUND_COMMAND, EXTERNAL_REQUEST), CurrentSafeModeReason telemetry, prepareForReboot port, ExternalFaultIgnoredInPayloadMode logging, error handling for state persistence operations, and comprehensive safe_mode_test.py tests |
| 2025-11-29 | Added low-voltage monitoring with debounced automatic payload exit, AutoPayloadModeExit event, and documentation/tests for aggressive load shed and manual exit power state |
| 2025-11-26 | Reordered enum values (SAFE=1, NORMAL=2, PAYLOAD=3) for sequential +1/-1 transitions; FORCE_SAFE_MODE now rejected from payload mode |
| 2025-11-26 | Removed forcePayloadMode port - payload mode now only entered via ENTER_PAYLOAD_MODE ground command |
| 2025-11-25 | Added PAYLOAD_MODE (commands, events, telemetry, persistence, payload load switch control) and documented payload integration tests |
| 2025-11-19 | Added getMode query port and enhanced modeChanged to carry mode value |

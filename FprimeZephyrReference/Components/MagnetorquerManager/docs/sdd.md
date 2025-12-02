# Components::MagnetorquerManager

The Magnetorquer Manager component coordinates the control of five magnetorquer faces on a PROVES CubeSat through output ports.

## Usage Examples

This component is designed to be used by the detumble service to be able to detumble the cubesat when launched.

### Diagrams

```mermaid
graph LR
    A[Detumble Service] -->|SetMagnetorquers| B[MagnetorquerManager]
    B -->|loadSwitchTurnOn| C[Load Switch 0<br/>X+ Face]
    B -->|loadSwitchTurnOn| D[Load Switch 1<br/>X- Face]
    B -->|loadSwitchTurnOn| E[Load Switch 2<br/>Y+ Face]
    B -->|loadSwitchTurnOn| F[Load Switch 3<br/>Y- Face]
    B -->|loadSwitchTurnOn| G[Load Switch 4<br/>Z+ Face]
    B -->|initDevice| H[Device Init Ports]
    B -->|triggerDevice| I[Device Trigger Ports]
```

### Typical Usage

1. The component is instantiated and initialized during system startup
2. The component's `configure()` method is called, which:
   - Enables all 5 load switches via `loadSwitchTurnOn` output ports
   - Initializes all 5 devices via `initDevice` output ports
   - Initializes the `enabled_faces` map with all faces set to false
3. The component is periodically called via the `run` scheduler port
4. The detumble service calls the `SetMagnetorquers` input port with an array of 5 key-value pairs to enable/disable specific faces (X+, X-, Y+, Y-, Z+)
5. On each `run` cycle:
   - The component checks if enabled
   - For each face that is enabled in `enabled_faces`, it calls the corresponding `triggerDevice` output port
6. When magnetorquers need to be disabled, the detumble service calls the `SetDisabled` input port

## Class Diagram

```mermaid
classDiagram
    class MagnetorquerManager {
        -string[5] faces
        -map~string, bool~ enabled_faces
        -bool enabled
        +configure()
        +SetMagnetorquers_handler(portNum, value)
        +SetDisabled_handler(portNum)
        +run_handler(portNum, context)
    }

    class MagnetorquerManagerComponentBase {
        <<F' Component Base>>
        +loadSwitchTurnOn_out(portNum)
        +initDevice_out(portNum, condition)
        +triggerDevice_out(portNum)
    }

    MagnetorquerManager --|> MagnetorquerManagerComponentBase
```

## Port Descriptions

### Input Ports

| Name             | Type             | Description                                                                                                                                                                  |
| ---------------- | ---------------- | ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| SetMagnetorquers | SetMagnetorquers | Input port that takes in an InputArray (array of 5 InputStruct) with key-value pairs where key=face name (string) and value=enabled status (bool). Faces: X+, X-, Y+, Y-, Z+ |
| SetDisabled      | SetDisabled      | Input port to disable all magnetorquers                                                                                                                                      |
| run              | Svc.Sched        | Scheduler port called periodically to trigger enabled magnetorquer devices                                                                                                   |

### Output Ports

| Name             | Type                | Count | Description                                                   |
| ---------------- | ------------------- | ----- | ------------------------------------------------------------- |
| loadSwitchTurnOn | Fw.Signal           | 5     | Output port for turning on load switches for each face        |
| initDevice       | Fw.SuccessCondition | 5     | Output port for initializing each device and receiving status |
| triggerDevice    | Drv.triggerDevice   | 5     | Output port for triggering haptic output on each device       |

## Sequence Diagrams

### Configure Operation

```mermaid
sequenceDiagram
    participant SYS as System Startup
    participant MM as MagnetorquerManager
    participant LS as Load Switches
    participant DEV as Devices

    SYS->>MM: configure()
    loop For each face (i = 0 to 4)
        MM->>LS: loadSwitchTurnOn_out(i)
    end
    loop For each face (i = 0 to 4)
        MM->>DEV: initDevice_out(i, condition)
        Note over MM: Initialize enabled_faces[faces[i]] = false
    end
    MM-->>SYS: Return
```

### SetMagnetorquers Operation

```mermaid
sequenceDiagram
    participant DS as Detumble Service
    participant MM as MagnetorquerManager
    participant Sched as Scheduler
    participant DEV as Downstream Devices

    DS->>MM: SetMagnetorquers([{key:"X+", value:true}, {key:"Y+", value:true}, ...])
    Note over MM: Set enabled = true
    loop For each InputStruct in array (5 total)
        Note over MM: Extract key (face name) and value (enabled)
        alt Face exists in enabled_faces
            Note over MM: Update enabled_faces[key] = value
        else Face not found
            MM->>MM: log_WARNING_HI_InvalidFace()
        end
    end
    MM-->>DS: Return

    Sched->>MM: run()
    Note over MM: Check if enabled
    loop For each face (i = 0 to 4)
        alt enabled_faces[faces[i]] == true
            MM->>DEV: triggerDevice_out(i)
        end
    end
    MM-->>Sched: Return
```

### SetDisabled Operation

```mermaid
sequenceDiagram
    participant DS as Detumble Service
    participant MM as MagnetorquerManager

    DS->>MM: SetDisabled()
    Note over MM: Set enabled = false
    loop For each face (i = 0 to 4)
        Note over MM: Set enabled_faces[faces[i]] = false
    end
    MM-->>DS: Return
```

## Commands

This component does not define any commands.

## Events

| Name        | Severity     | Description                                                                                                              |
| ----------- | ------------ | ------------------------------------------------------------------------------------------------------------------------ |
| InvalidFace | Warning High | Output when SetMagnetorquers receives a face name that doesn't exist in enabled_faces (valid faces: X+, X-, Y+, Y-, Z+). |

## Requirements

| Name                    | Description                                                                                                            | Validation       |
| ----------------------- | ---------------------------------------------------------------------------------------------------------------------- | ---------------- |
| MagnetorquerManager-001 | The Magnetorquer Manager enables all load switches and initializes all 5 devices during configuration                  | Integration Test |
| MagnetorquerManager-002 | The SetMagnetorquers port allows other components to configure the on/off state of specific faces (X+, X-, Y+, Y-, Z+) | Integration Test |
| MagnetorquerManager-003 | The SetDisabled port disables all magnetorquers by setting enabled to false and clearing enabled_faces                 | Integration Test |
| MagnetorquerManager-004 | The run handler is called on a rate group to trigger devices for enabled faces via triggerDevice output ports          | Integration Test |
| MagnetorquerManager-005 | The component logs InvalidFace event when an unknown face name is provided to SetMagnetorquers                         | Unit Test        |

## Change Log

| Date       | Description            |
| ---------- | ---------------------- |
| 12/01/2025 | Initial implementation |

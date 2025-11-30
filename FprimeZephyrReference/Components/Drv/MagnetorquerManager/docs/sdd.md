# Drv::MagnetorquerManager

The Magnetorquer Manager component interfaces with the five DRV2605 devices on a PROVES CubeSat to control the current of the magnetorquers.

## Usage Examples

This component is designed to be used by the detumble service to be able to detumble the cubesat when launched.

### Diagrams

```mermaid
graph LR
    A[Detumble Service] -->|SetMagnetorquers| B[MagnetorquerManager]
    B -->|Configure| C[DRV2605 Device 0<br/>X1 Face]
    B -->|Configure| D[DRV2605 Device 1<br/>X2 Face]
    B -->|Configure| E[DRV2605 Device 2<br/>Y1 Face]
    B -->|Configure| F[DRV2605 Device 3<br/>Y2 Face]
    B -->|Configure| G[DRV2605 Device 4<br/>Z1 Face]
```

### Typical Usage

1. The component is instantiated and initialized during system startup
2. The component is configured with a map of DRV2605 devices (face name -> device pointer) via the `configure()` method
3. The component is periodically called via the `run` scheduler port
4. The detumble service calls the `SetMagnetorquers` input port with an array of key-value pairs to enable/disable specific faces
5. On each `run` cycle:
   - The component checks if enabled
   - The component iterates through the device map
   - For each face that is enabled in `enabled_faces`, it starts haptic output on the corresponding device
6. When magnetorquers need to be disabled, the detumble service calls the `SetDisabled` input port

## Class Diagram

```mermaid
classDiagram
    class MagnetorquerManager {
        -map~string, device*~ m_devices
        -drv2605_config_data config_data
        -bool enabled
        -map~string, bool~ enabled_faces
        +configure(map~string, device*~)
        +SetMagnetorquers_handler(portNum, value)
        +SetDisabled_handler(portNum)
        +run_handler(portNum, context)
    }

    class MagnetorquerManagerComponentBase {
        <<F' Component Base>>
    }

    class DRV2605 {
        <<Zephyr Driver>>
        +device_is_ready()
        +drv2605_haptic_config()
        +haptics_start_output()
        +haptics_stop_output()
    }

    MagnetorquerManager --|> MagnetorquerManagerComponentBase
    MagnetorquerManager --> DRV2605 : uses (multiple devices)
```

## Port Descriptions

| Name             | Description                                                                                                                                                                                        |
| ---------------- | -------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| SetMagnetorquers | Input port that takes in an InputArray (array of InputStruct) with key-value pairs where key=face name (string) and value=enabled status (bool). Supports up to 10 face entries for flexibility. |
| SetDisabled      | Input port to disable all magnetorquers and stop haptic output                                                                                                                                     |
| run              | Scheduler port called periodically to start haptic output on enabled faces                                                                                                                         |

## Sequence Diagrams

### SetMagnetorquers Operation

```mermaid
sequenceDiagram
    participant DS as Detumble Service
    participant MM as MagnetorquerManager
    participant Sched as Scheduler
    participant DRV as DRV2605 Devices

    DS->>MM: SetMagnetorquers([{key:"x1", value:true}, {key:"y1", value:true}, ...])
    Note over MM: Set enabled = true
    loop For each InputStruct in array
        Note over MM: Extract key (face name) and value (enabled)
        alt Face exists in m_devices
            Note over MM: Update enabled_faces[key] = value
        else Face not found
            MM->>MM: log_WARNING_HI_InvalidFace()
        end
    end
    MM-->>DS: Return

    Sched->>MM: run()
    Note over MM: Check if enabled
    loop For each (key, device) in m_devices
        alt enabled_faces[key] == true
            MM->>DRV: Check device_is_ready()
            alt Device Ready
                MM->>DRV: haptics_start_output()
            else Device Not Ready
                Note over MM: Skip this device
            end
        end
    end
    MM-->>Sched: Return
```

### SetDisabled Operation

```mermaid
sequenceDiagram
    participant DS as Detumble Service
    participant MM as MagnetorquerManager
    participant DRV as DRV2605 Devices

    DS->>MM: SetDisabled()
    Note over MM: Set enabled = false
    loop For each (key, device) in m_devices
        MM->>DRV: Check device_is_ready()
        alt Device Ready
            MM->>DRV: haptics_stop_output()
            Note over MM: Set enabled_faces[key] = false
        else Device Not Ready
            MM->>MM: log_WARNING_HI_DeviceNotReady()
        end
    end
    MM-->>DS: Return
```

## Commands

This component does not define any commands.

## Events

| Name                | Description                                                                     |
| ------------------- | ------------------------------------------------------------------------------- |
| DeviceNotReady      | Output whenever a magnetorquer is attempted to be used while it is not ready.   |
| DeviceNotInitialized | Output when a DRV2605 device fails to initialize during configuration.         |
| InvalidFace         | Output when SetMagnetorquers receives a face name that doesn't exist in m_devices. |

## Requirements

| Name                    | Description                                                                                       | Validation       |
| ----------------------- | ------------------------------------------------------------------------------------------------- | ---------------- |
| MagnetorquerManager-001 | The Magnetorquer Manager configures all DRV2605 devices on initialization                         | Integration Test |
| MagnetorquerManager-002 | The SetMagnetorquers port allows other components to configure the on/off state of specific faces | Integration Test |
| MagnetorquerManager-003 | The SetDisabled port disables all magnetorquers and stops haptic output                           | Integration Test |
| MagnetorquerManager-004 | The run handler is called on a rate group to start haptic output on enabled faces                 | ?                |

## Change Log

| Date       | Description            |
| ---------- | ---------------------- |
| 11/30/2025 | Initial implementation |

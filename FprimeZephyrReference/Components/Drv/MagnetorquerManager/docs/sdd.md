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
2. The component is configured with the DRV2605 devices via the `configure()` method
3. The component is periodically called via the `run` scheduler port
4. The detumble service calls the `SetMagnetorquers` input port to enable/disable specific faces
5. On each `run` cycle:
   - The component checks which faces are enabled
   - For each enabled face, it starts haptic output on the corresponding device
6. When magnetorquers need to be disabled, the detumble service calls the `SetDisabled` input port

## Class Diagram

```mermaid
classDiagram
    class MagnetorquerManager {
        -device* m_devices[5]
        -drv2605_config_data config_data
        -bool enabled
        -bool enabled_faces[5]
        +configure(devices[5])
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
    MagnetorquerManager --> DRV2605 : uses (5 devices)
```

## Port Descriptions

| Name             | Description                                                                                  |
| ---------------- | -------------------------------------------------------------------------------------------- |
| SetMagnetorquers | Input port that takes in an array (bool[5]) to enable/disable each face (x1, x2, y1, y2, z1) |
| SetDisabled      | Input port to disable all magnetorquers and stop haptic output                               |
| run              | Scheduler port called periodically to start haptic output on enabled faces                   |

## Sequence Diagrams

### SetMagnetorquers Operation

```mermaid
sequenceDiagram
    participant DS as Detumble Service
    participant MM as MagnetorquerManager
    participant Sched as Scheduler
    participant DRV as DRV2605 Devices

    DS->>MM: SetMagnetorquers([true, false, true, false, true])
    Note over MM: Set enabled = true
    Note over MM: Update enabled_faces array
    MM-->>DS: Return

    Sched->>MM: run()
    Note over MM: Check if enabled
    loop For each enabled face (0-4)
        alt enabled_faces[i] == true
            MM->>DRV: Check device_is_ready()
            alt Device Ready
                MM->>DRV: haptics_start_output()
            else Device Not Ready
                MM->>MM: log_WARNING_HI_DeviceNotReady()
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
    loop For each face (0-4)
        MM->>DRV: Check device_is_ready()
        alt Device Ready
            MM->>DRV: haptics_stop_output()
            Note over MM: Set enabled_faces[i] = false
        else Device Not Ready
            MM->>MM: log_WARNING_HI_DeviceNotReady()
        end
    end
    MM-->>DS: Return
```

## Commands

This component does not define any commands.

## Events

| Name           | Description                                                                   |
| -------------- | ----------------------------------------------------------------------------- |
| DeviceNotReady | Output whenever a magnetorquer is attempted to be used while it is not ready. |

## Requirements

Add requirements in the chart below
| Name | Description | Validation |
|---|---|---|
|---|---|---|

## Change Log

| Date       | Description            |
| ---------- | ---------------------- |
| 11/25/2025 | Initial implementation |

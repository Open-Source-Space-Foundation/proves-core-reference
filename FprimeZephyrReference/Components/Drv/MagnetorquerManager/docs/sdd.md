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
2. The detumble service calls the `SetMagnetorquers` input port.
3. On each call, the component:
   - Takes in an `InputArray` parameter of 5 I32 for the amps for each face.
   - Translates the passed in values to a sequence value from the DRV2605 library.
   - Runs the sequence for each device.

## Class Diagram

```mermaid
classDiagram
    class MagnetorquerManager {
        -device* m_devices[5]
        +configure(devices[5])
        +SetMagnetorquers_handler(portNum, value)
        +START_PLAYBACK_TEST_cmdHandler(opCode, cmdSeq, faceIdx)
        +START_PLAYBACK_TEST2_cmdHandler(opCode, cmdSeq, faceIdx)
    }

    class MagnetorquerManagerComponentBase {
        <<F' Component Base>>
    }

    class DRV2605 {
        <<Zephyr Driver>>
        +device_is_ready()
        +drv2605_haptic_config()
    }

    MagnetorquerManager --|> MagnetorquerManagerComponentBase
    MagnetorquerManager --> DRV2605 : uses (5 devices)
```

## Port Descriptions

| Name             | Description                                                                                 |
| ---------------- | ------------------------------------------------------------------------------------------- |
| SetMagnetorquers | Input port that takes in an array (I32[5]) and applies each value to the corresponding face |

## Sequence Diagrams

### SetMagnetorquers Operation

```mermaid
sequenceDiagram
    participant DS as Detumble Service
    participant MM as MagnetorquerManager
    participant DRV as DRV2605 Devices

    DS->>MM: SetMagnetorquers([x1, x2, y1, y2, z1])
    Note over MM: Validate input array
    Note over MM: Translate amps to sequences
    loop For each face (0-4)
        MM->>DRV: Check device_is_ready()
        alt Device Ready
            MM->>DRV: drv2605_haptic_config(sequence)
        else Device Not Ready
            MM->>MM: log_WARNING_HI_DeviceNotReady()
        end
    end
    MM-->>DS: Return
```

### Test Command Operation

```mermaid
sequenceDiagram
    participant GS as Ground Station
    participant MM as MagnetorquerManager
    participant DRV as DRV2605 Device

    GS->>MM: START_PLAYBACK_TEST(faceIdx)
    MM->>MM: Validate faceIdx (0-4)
    alt Invalid faceIdx
        MM->>MM: log_WARNING_LO_InvalidFaceIndex()
    else Valid faceIdx
        MM->>DRV: device_is_ready()
        alt Device Ready
            MM->>DRV: drv2605_haptic_config(effect #47)
        else Device Not Ready
            MM->>MM: log_WARNING_HI_DeviceNotReady()
        end
    end
    MM-->>GS: Command Response
```

## Commands

| Name                 | Description                                                                                      |
| -------------------- | ------------------------------------------------------------------------------------------------ |
| START_PLAYBACK_TEST  | Start DRV2605 playback on a device with effect #47 on a specific face (faceIdx: 0-4). Test only. |
| START_PLAYBACK_TEST2 | Start DRV2605 playback on a device with effect #50 on a specific face (faceIdx: 0-4). Test only. |

## Events

| Name             | Description                                                                                                                                 |
| ---------------- | ------------------------------------------------------------------------------------------------------------------------------------------- |
| DeviceNotReady   | Output whenever a magnetorquer is attempted to be used while it is not initialized.                                                         |
| InvalidFaceIndex | Output whenever one of the manual test comamands are ran with an invalid face index (will be removed if/when the test commands are removed) |

## Requirements

Add requirements in the chart below
| Name | Description | Validation |
|---|---|---|
|---|---|---|

## Change Log

| Date       | Description   |
| ---------- | ------------- |
| 11/11/2025 | Initial Draft |

# Components::RtcManager

The RTC Manager component interfaces with the Real Time Clock (RTC) to provide time measurements. When the RTC is unavailable, the component automatically fails over to monotonic time (uptime since boot) to ensure continuous system operation.

### Typical Usage

#### `TIME_SET` Command Usage
1. The component is instantiated and initialized during system startup
2. A ground station sends a `TIME_SET` command with the desired time
3. On each command, the component:
    - Validates the time data (year >= 1900, month [1-12], day [1-31], hour [0-23], minute [0-59], second [0-59])
    - Emits validation failure events if any field is invalid
    - Sets the time on the RTC if validation passes
    - Emits a `TimeSet` event with the previous time if the time is set successfully
    - Emits a `TimeNotSet` event if the time is not set successfully
    - Emits a `DeviceNotReady` event if the device is not ready

#### `ALARM_SET` Command Usage
1. The component is instantiated and initialized during system startup
2. A ground station sends a `ALARM_SET` command with the desired time
3. On each command, the component:
    - Validates that the alarm is at a future date
    - Emits 'AlarmNotSet' if time is not valid or if an alarm is already present
    - Sets the time to the RTC alarm if validation passes
    - Emits a `AlarmSet` event with the previous time if the alarm is set successfully
    - Emits a `AlarmNotSet` event if the alarm is not set successfully

#### `ALARM_CANCEL` Command Usage
1. A ground station sends a `ALARM_CANCEL` command with the desired alarm ID to cancel
2. On each command, the component:
    - Validates that an alarm exists
    - Emits 'AlarmNotCanceled' if an alarm is already present
    - Cancels the present alarm if it is present on the system
    - Emits a `AlarmCanceled` event with the previous time if the alarm is canceled successfully
    - Emits a `AlarmNotCanceled` event if the alarm is not canceled successfully

#### `ALARM_LIST` Command Usage
1. A ground station sends a `ALARM_LIST` command
2. On each command, the component:
    - Validates that an alarm exists
    - Emits 'AlarmNotSet' if no alarm is present
    - Emits 'AlarmSet' if an alarm is present and returns it's information to the ground station

#### `timeGetPort` Port Usage
1. The component is instantiated and initialized during system startup
2. In a deployment topology, a `time connection` relation is made to sync FPrime's internal clock
3. On each call, the component:
    - Checks if the RTC device is ready
    - If the RTC is ready:
        - Fetches time from the RTC hardware
        - Computes a rescaled microsecond value using the system cycle counter
        - Returns time with `TB_WORKSTATION_TIME` time base
    - If the RTC is not ready (failover mode):
        - Logs a warning message (throttled to prevent console flooding)
        - Fetches monotonic uptime from the system
        - Computes a rescaled microsecond value using the system cycle counter
        - Returns time with `TB_PROC_TIME` time base (uptime since boot)
    - Uses an internal offset and modulo arithmetic to keep microseconds in `[0, 999_999]` while ensuring sub-second monotonicity for each time base

### Sub-second Monotonicity Behavior

To ensure that `Fw::Time` always has a valid, non-decreasing microsecond field:

- On the first call to the `timeGetPort` for any one second of time passed on the real time clock:
  - Captures microseconds since boot value derived from the hardware cycle counter
  - Stores this as `m_offset_useconds`
- For each subsequent `timeGetPort` call during the same second:
  - Reads the current microseconds since boot from the cycle counter
  - Forms an adjusted value `(current_useconds - m_offset_useconds)` and applies modulo `1_000_000`
  - Uses the result as the `useconds` field in `Fw::Time`

This guarantees:

- `0 <= useconds <= 999_999` for all returned times (satisfies `FW_ASSERT(useconds < 1000000, ...)`)
- No backward jumps in the sub-second field for a given time base, until natural wrap at one second

This logic applies both when using the RTC (`TB_WORKSTATION_TIME`) and when in failover mode using uptime (`TB_PROC_TIME`).

## Requirements
| Name | Description | Validation |
|---|---|---|
| RtcManager-001 | The RTC Manager has a command that sets the time on the RTC | Integration test |
| RtcManager-002 | The RTC Manager has a port which, when called, returns the time from the RTC or uptime | Integration test |
| RtcManager-003 | In the event of an error retrieving time from the RTC, the RTC Manager returns uptime | Integration test |
| RtcManager-004 | A time set event is emitted if the time is set successfully, including the previous time | Integration test |
| RtcManager-005 | A time not set event is emitted if the time is not set successfully | Integration test |
| RtcManager-006 | The RTC Manager validates time data and emits validation failure events for invalid fields | Integration test |
| RtcManager-007 | Time increments continuously regardless of RTC availability | Integration test |
| RtcManager-008 | The sub-second microseconds field is always in the range [0, 999999] | Unit tests |
| RtcManager-009 | Time is monotonic | Integration test |
| RtcManager-010 | During a time set command, before the new time is set, RTC Manager informs a sequence cancellation port | Integration test |
| RtcManager-011 | An alarm is set and then an event is emitted when the alarm triggers | Integration test |
| RtcManager-012 | An alarm is set and then canceled, an event is emitted when the alarm is canceled | Integration test |
| RtcManager-013 | An alarm cancel command is sent when no alarm is present and an event is emitted | Integration test |
| RtcManager-014 | Alarm list is tested before and after an alarm is set to ensure proper behavior | Integration test |
| RtcManager-015 | Alarm is set with an impossible time and an event is emitted, the alarm is not set | Integration test |
| RtcManager-016 | Alarm is set and then another alarm is set. An event is emitted and the second alarm is not set | Integration test |


## Port Descriptions
| Name | Description |
|---|---|
| timeGetPort | Time port for FPrime topology connection to get the time from the RTC |
| alarmTriggered | Output port to keep track of when an alarm triggers |

## Commands
| Name | Description |
|---|---|
| TIME_SET | Sets the time on the RTC with validation of all time fields |
| ALARM_SET | Sets the RTC alarm with as much precision as hardware allows |
| ALARM_CANCEL | Cancels the current alarm |
| ALARM_LIST | Responds with info about the current set alarm |

## Events
| Name | Description |
|---|---|
| DeviceNotReady | Emitted when the RTC device is not ready during TIME_SET command |
| TimeSet | Emitted on successful time set, includes previous time (seconds and microseconds) |
| TimeNotSet | Emitted on unsuccessful time set or if one exists when alarm list is run |
| AlarmSet | Emitted when alarm is successfully set |
| AlarmNotSet | Emitted when alarm cannot be set or if it is not set when alarm list is run |
| AlarmTriggered | Emitted when an alarm fires |
| AlarmCanceled | Emitted when an alarm is canceled |
| AlarmNotCanceled | Emitted when an alarm cannot be canceled |
| YearValidationFailed | Emitted when provided year is invalid (should be >= 1900) |
| MonthValidationFailed | Emitted when provided month is invalid (should be [1-12]) |
| DayValidationFailed | Emitted when provided day is invalid (should be [1-31]) |
| HourValidationFailed | Emitted when provided hour is invalid (should be [0-23]) |
| MinuteValidationFailed | Emitted when provided minute is invalid (should be [0-59]) |
| SecondValidationFailed | Emitted when provided second is invalid (should be [0-59]) |

## Class Diagram

### RTC Manager Class Diagram
```mermaid
classDiagram
    namespace Drv {
        class RtcManagerComponentBase {
            <<Auto-generated>>
        }
        class RtcManager {
            - m_dev: const device*
            - m_rtcHelper: RtcHelper
            - m_RtcNotReadyThrottle: atomic~bool~
            - m_RtcGetTimeFailedThrottle: atomic~bool~
            - m_RtcInvalidTimeThrottle: atomic~bool~
            - m_curr_mask: U16
            - m_alarm_time: rtc_time

            + RtcManager(const char* const compName)
            + ~RtcManager()
            + configure(dev: const device*) void

            - timeGetPort_handler(portNum: FwIndexType, time: Fw::Time&) void

            - TIME_SET_cmdHandler(opCode: FwOpcodeType, cmdSeq: U32, t: Drv::TimeData) void
            - ALARM_SET_cmdHandler(opCode: FwOpcodeType, cmdSeq: U32, t: Drv::TimeData) void
            - ALARM_CANCEL_cmdHandler(opCode: FwOpcodeType, cmdSeq: U32, ID: U16) void
            - ALARM_LIST_cmdHandler(opCode: FwOpcodeType, cmdSeq: U32) void

            - static_alarm_callback_t(dev: const device*, id: uint16_t, user_data: void*) void$
            - alarm_callback_t(dev: const device*, id: uint16_t) void
            - log_CONSOLE_RtcNotReady() void
            - log_CONSOLE_RtcNotReady_ThrottleClear() void
            - log_CONSOLE_RtcGetTimeFailed(rc: int) void
            - log_CONSOLE_RtcGetTimeFailed_ThrottleClear() void
            - log_CONSOLE_RtcInvalidTime() void
            - log_CONSOLE_RtcInvalidTime_ThrottleClear() void
            - timeDataIsValid(t: Drv::TimeData) bool
        }
    }
    RtcManagerComponentBase <|-- RtcManager : inherits
    RtcManager *-- RtcHelper : uses
```

### RTC Helper Class Diagram
```mermaid
classDiagram
    namespace Drv {
        class RtcHelper {
            - m_last_seen_seconds: uint32_t = 0
            - m_useconds_offset: uint32_t = 0

            + RtcHelper()
            + ~RtcHelper()
            + uint32_t rescaleUseconds(current_seconds: uint32_t, current_useconds: uint32_t)
        }
    }
```

## Sequence Diagrams

### `timeGetPort` port

The `timeGetPort` port is called from a `time connection` in a deployment topology to sync the RTC's time with FPrime's internal clock. The component automatically falls back to uptime if the RTC is unavailable.

#### Success (RTC Available)

```mermaid
sequenceDiagram
    participant Deployment Time Connection
    participant RTC Manager
    participant System Clock
    participant Zephyr RTC API
    participant RTC Sensor

    Deployment Time Connection->>RTC Manager: Call timeGetPort time port
    RTC Manager->>System Clock: Get uptime via k_uptime_get()
    System Clock-->>RTC Manager: Return uptime in ms
    RTC Manager->>RTC Manager: Check RTC device_is_ready()
    Note over RTC Manager: Device is ready
    RTC Manager->>RTC Sensor: Get real time in seconds
    RTC Manager->>Zephyr RTC API: Read time via rtc_get_time()
    Zephyr RTC API->>RTC Sensor: Read time
    RTC Sensor-->>Zephyr RTC API: Return time
    Zephyr RTC API-->>RTC Manager: Return rtc_time struct
    RTC Manager->>RTC Manager: Convert to time_t via timeutil_timegm()
    RTC Manager->>RTC Helper: Combine RTC and System clock data to get monotonicly increasing useconds
    RTC Helper-->>RTC Manager: Return useconds
    RTC Manager-->>Deployment Time Connection: Return Fw::Time with time base `TB_WORKSTATION_TIME`
```

#### Failover to Monotonic Time (RTC Unavailable)

```mermaid
sequenceDiagram
    participant Console Log
    participant Deployment Time Connection
    participant RTC Manager
    participant System Clock

    Deployment Time Connection->>RTC Manager: Call timeGetPort time port
    RTC Manager->>System Clock: Get uptime via k_uptime_get()
    System Clock-->>RTC Manager: Return uptime in ms
    RTC Manager->>RTC Manager: Check RTC device_is_ready()
    Note over RTC Manager: Device not ready
    RTC Manager->>Console Log: Log "RTC not ready" (throttled)
    RTC Manager-->>Deployment Time Connection: Return Fw::Time with time base `TB_PROC_TIME`
```

### `TIME_SET` Command

The `TIME_SET` command is called to set the current time on the RTC. The component validates all time fields before attempting to set the time.

#### Success

```mermaid
sequenceDiagram
    participant Ground Station
    participant Event Log
    participant RTC Manager
    participant Zephyr RTC API
    participant RTC Sensor

    Ground Station->>RTC Manager: Command TIME_SET with Drv::TimeData struct
    RTC Manager->>RTC Manager: Check device_is_ready()
    RTC Manager->>RTC Manager: Validate time data (timeDataIsValid)
    Note over RTC Manager: Check year >= 1900<br/>month [1-12], day [1-31]<br/>hour [0-23], min [0-59], sec [0-59]
    RTC Manager->>RTC Manager: Store previous time via getTime()
    RTC Manager->>Zephyr RTC API: Set time via rtc_set_time()
    Zephyr RTC API->>RTC Sensor: Set time
    RTC Sensor-->>Zephyr RTC API: Return success
    Zephyr RTC API-->>RTC Manager: Return success (status = 0)
    RTC Manager->>Event Log: Emit TimeSet event (with previous time)
    RTC Manager-->>Ground Station: Command response OK
```

#### Validation Failure

```mermaid
sequenceDiagram
    participant Ground Station
    participant Event Log
    participant RTC Manager

    Ground Station->>RTC Manager: Command TIME_SET with invalid Drv::TimeData
    RTC Manager->>RTC Manager: Check device_is_ready()
    RTC Manager->>RTC Manager: Validate time data (timeDataIsValid)
    Note over RTC Manager: Validation fails
    RTC Manager->>Event Log: Emit validation failure events<br/>(YearValidationFailed, etc.)
    RTC Manager->>Event Log: Emit TimeNotSet event
    RTC Manager-->>Ground Station: Command response VALIDATION_ERROR
```

#### Device Not Ready

```mermaid
sequenceDiagram
    participant Ground Station
    participant Event Log
    participant RTC Manager

    Ground Station->>RTC Manager: Command TIME_SET with Drv::TimeData struct
    RTC Manager->>RTC Manager: Check device_is_ready()
    Note over RTC Manager: Device not ready
    RTC Manager->>Event Log: Emit DeviceNotReady event
    RTC Manager-->>Ground Station: Command response EXECUTION_ERROR
```

#### Time Not Set (RTC Failure)

```mermaid
sequenceDiagram
    participant Ground Station
    participant Event Log
    participant RTC Manager
    participant Zephyr RTC API
    participant RTC Sensor

    Ground Station->>RTC Manager: Command TIME_SET with Drv::TimeData struct
    RTC Manager->>RTC Manager: Check device_is_ready()
    RTC Manager->>RTC Manager: Validate time data (timeDataIsValid)
    RTC Manager->>Zephyr RTC API: Set time via rtc_set_time()
    Zephyr RTC API->>RTC Sensor: Set time
    RTC Sensor-->>Zephyr RTC API: Return failure
    Zephyr RTC API-->>RTC Manager: Return failure (status != 0)
    RTC Manager->>Event Log: Emit TimeNotSet event
    RTC Manager-->>Ground Station: Command response EXECUTION_ERROR
```

### `ALARM_SET` Command

The `ALARM_SET` command is called to set the current time on the RTC alarm. The component validates that the time is in the future before setting the alarm

#### Success

```mermaid
sequenceDiagram
    participant Ground Station
    participant Event Log
    participant RTC Manager
    participant Zephyr RTC API
    participant RTC Sensor

    Ground Station->>RTC Manager: Command ALARM_SET with Drv::TimeData struct
    RTC Manager->>RTC Manager: Validate alarm not present on system
    RTC Manager->>RTC Manager: Validate time data (timeDataIsValid)
    RTC Manager->>Zephyr RTC API: Set alarm time via rtc_alarm_set_time()
    Zephyr RTC API->>RTC Sensor: Set rtc alarm time
    RTC Sensor-->>Zephyr RTC API: Return success
    Zephyr RTC API-->>RTC Manager: Return success (status = 0)
    RTC Manager->>Event Log: Emit AlarmSet event (with previous time)
    RTC Manager-->>Ground Station: Command response OK
```

#### Failure

```mermaid
sequenceDiagram
    participant Ground Station
    participant Event Log
    participant RTC Manager
    participant Zephyr RTC API
    participant RTC Sensor

    Ground Station->>RTC Manager: Command ALARM_SET with Drv::TimeData struct
    RTC Manager->>RTC Manager: Validate alarm not present on system
    RTC Manager->>RTC Manager: Validate time data (timeDataIsValid)
    RTC Manager->>Zephyr RTC API: Set alarm time via rtc_alarm_set_time()
    Zephyr RTC API->>RTC Sensor: Set rtc alarm time
    RTC Sensor-->>Zephyr RTC API: Return failure
    Zephyr RTC API-->>RTC Manager: Return failure (nonzero return code)
    RTC Manager->>Event Log: Emit AlarmNotSet event (with previous time)
    RTC Manager-->>Ground Station: Command response Execution Error
```

### `ALARM_CANCEL` Command

The `ALARM_CANCEL` command is called to cancel the RTC alarm. component validates that an alarm is present and can be accessed

#### Success

```mermaid
sequenceDiagram
    participant Ground Station
    participant Event Log
    participant RTC Manager
    participant Zephyr RTC API
    participant RTC Sensor

    Ground Station->>RTC Manager: Command ALARM_CANCEL with Uint16_t ID
    RTC Manager->>RTC Manager: Validate alarm is present on system
    RTC Manager->>Zephyr RTC API: Set alarm mask to 0 via rtc_alarm_set_time()
    Zephyr RTC API->>RTC Sensor: Set rtc alarm time
    RTC Sensor-->>Zephyr RTC API: Return success
    Zephyr RTC API-->>RTC Manager: Return success (status = 0)
    RTC Manager->>Event Log: Emit AlarmCanceled event (with ID)
    RTC Manager-->>Ground Station: Command response OK
```

#### Failure

```mermaid
sequenceDiagram
    participant Ground Station
    participant Event Log
    participant RTC Manager
    participant Zephyr RTC API
    participant RTC Sensor

    Ground Station->>RTC Manager: Command ALARM_CANCEL with Uint16_t ID
    RTC Manager->>RTC Manager: Validate alarm is present on system
    RTC Manager->>Zephyr RTC API: Set alarm mask to 0 via rtc_alarm_set_time()
    Zephyr RTC API->>RTC Sensor: Set rtc alarm time
    RTC Sensor-->>Zephyr RTC API: Return Failure
    Zephyr RTC API-->>RTC Manager: Return Failure (nonzero return code)
    RTC Manager->>Event Log: Emit AlarmNotCanceled event (with ID)
    RTC Manager-->>Ground Station: Command response Execution error
```

### `ALARM_LIST` Command

The `ALARM_LIST` command is called to retrieve information about the rtc alarm's current status

#### Present

```mermaid
sequenceDiagram
    participant Ground Station
    participant Event Log
    participant RTC Manager
    participant Zephyr RTC API
    participant RTC Sensor

    Ground Station->>RTC Manager: Command ALARM_LIST
    RTC Manager->>RTC Manager: Determine whether an alarm is present or not with rtc_alarm_get_time()
    RTC Manager->>Zephyr RTC API: retrieve alarm information (time, ID, mask)
    Zephyr RTC API->>RTC Sensor: retrieve alarm information (time, ID, mask)
    RTC Sensor-->>Zephyr RTC API: return mask
    Zephyr RTC API-->>RTC Manager: return mask and determine whether it is zero
    RTC Manager->>Event Log: Emit alarmSet event with the information of the present alarm
    RTC Manager-->>Ground Station: Command response OK
```

#### Absent

```mermaid
sequenceDiagram
    participant Ground Station
    participant Event Log
    participant RTC Manager
    participant Zephyr RTC API
    participant RTC Sensor

    Ground Station->>RTC Manager: Command ALARM_LIST
    RTC Manager->>RTC Manager: Determine whether an alarm is present or not with rtc_alarm_get_time()
    RTC Manager->>Zephyr RTC API: retrieve alarm information (time, ID, mask)
    Zephyr RTC API->>RTC Sensor: retrieve alarm information (time, ID, mask)
    RTC Sensor-->>Zephyr RTC API: return mask
    Zephyr RTC API-->>RTC Manager: return mask and determine whether it is zero
    RTC Manager->>Event Log: Emit alarmNotSet event
    RTC Manager-->>Ground Station: Command response OK
```

## Change Log

| Date | Description |
|---|---|
| 2025-9-18 | Initial RTC Manager component |
| 2025-11-14 | Added monotonic time failover when RTC unavailable, input validation for TIME_SET command, TEST_UNCONFIGURE_DEVICE test command, and console logging for device not ready conditions |
| 2025-12-26 | Ensured sub-second time is monotonic; added unit tests for sub-second time calculation; removed TEST_UNCONFIGURE_DEVICE |
| 2026-04-02 | Added basic functionality for setting and canceling RTC alarms |
| 2026-04-09 | Hardening for more consistent behavior |

# Components::Rv3028Driver

Drives the real time clock

## Requirements
Add requirements in the chart below
| Name | Description | Validation |
|---|---|---|
| Rv3028Driver-001 | Time can be set on the RV3028 through a port | Manual |
| Rv3028Driver-002 | Time can be read from the RV3028 through a port | Manual |
| Rv3028Driver-003 | A device not ready event is emitted if the RV3028 is not ready | Manual |
| Rv3028Driver-004 | A time set event is emitted if the time is set successfully | Manual |
| Rv3028Driver-005 | A time not set event is emitted if the time is not set successfully | Manual |

## Port Descriptions
| Name | Description |
|---|---|
| timeSet | Input port sets the time on the RV3028 |
| timeRead | Input port reads the time from the RV3028 |

## Sequence Diagrams
Add sequence diagrams here

## Events
| Name | Description |
|---|---|
| DeviceNotReady | Emits on unsuccessful device connection |
| TimeSet | Emits on successful time set |
| TimeNotSet | Emits on unsuccessful time set |

# Components::Rv3028Driver

Drives the real time clock

## Requirements
Add requirements in the chart below
| Name | Description | Validation |
|---|---|---|
| Rv3028Driver-001 | Time can be set on the RTC through a port  | Manual |
| Rv3028Driver-002 | Time can be gotten through a port | Manual |
| Rv3028Driver-003 | Not ready event and set event will be emitted from the driver | Manual |
| Rv3028Driver-003 | The time from the Rv3028 Driver will set the time in F Prime | Manual |

## Port Descriptions
| Name | Description |
|---|---|
| SET_TIME (input) | Port that receives a command to set the time |
| timeCaller (time) | Output port to send the time  |
|GET_TIME| Port that sends the time|


## Sequence Diagrams
Add sequence diagrams here

## Events
| Name | Description |
|---|---|
| RTC_Set | Transmits success or failure RTC time when it is reset |
| DeviceNotReady | Emits on unsuccessful device connection |


## Unit Tests
Add unit test descriptions in the chart below
| Name | Description | Output | Coverage |
|---|---|---|---|
|---|---|---|---|

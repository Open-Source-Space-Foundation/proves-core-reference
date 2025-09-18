# Components::RtcManager

Manages the real time clock

## Usage Examples
Add usage examples here

### Diagrams
Add diagrams here

### Typical Usage
And the typical usage of the component here

## Class Diagram
Add a class diagram here

## Requirements
Add requirements in the chart below
| Name | Description | Validation |
|---|---|---|
| RtcManager-001 | The RTC manager shall have a command to set and get times from the RTC Driver | Manual |
| RtcManager-002 | An Event is Emitted when time is gotten with the time | Manual |

## Port Descriptions
| Name | Description |
|---|---|
| SetTime (output) | Port to reach out to driver to set the time |
| TimeSend (input) | Port to receive the time from driver |
| TimeRead (output) | Port to reach out to driver to ask for the time |
| timeCaller (time) | Port to receive time |

## Commands
| Name | Description |
|---|---|
| SET_TIME | Sets the time on the RTC |
| GET_TIME | Gets the time from the RTC |


## Events
| Name | Description |
|---|---|
| RTC_GetTime | Transmits time gotten from the driver |


## Unit Tests
Add unit test descriptions in the chart below
| Name | Description | Output | Coverage |
|---|---|---|---|
|---|---|---|---|


## Change Log
| Date | Description |
|---|---|
| 2025-09-17 | Initial Draft |

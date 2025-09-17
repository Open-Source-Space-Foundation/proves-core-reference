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
| RtcManager-001 | Caller can set time on the RTC | Manual |
| RtcManager-002 | Caller can receive time from FPrime | Manual |
| RtcManager-003 | Event when time is set, report success or failure | Manual |

## Port Descriptions
| Name | Description |
|---|---|
| SetTime (output) | Port to reach out to driver to set the time |
| timeCaller (time) | Port to receive time |

## Component States
Add component states in the chart below
| Name | Description |
|---|---|
|---|---|

## Sequence Diagrams
Add sequence diagrams here

## Parameters
| Name | Description |
|---|---|
|---|---|

## Commands
| Name | Description |
|---|---|
| SET_TIME | Sets the time on the RTC |
| GET_TIME | Gets the time from the RTC |


## Events
| Name | Description |
|---|---|
| RTC_Set | Transmits success or failure RTC time when it is reset |
| RTC_NotReady | Emits on unsuccessful device connection |


## Telemetry
| Name | Description |
|---|---|
|---|---|

## Unit Tests
Add unit test descriptions in the chart below
| Name | Description | Output | Coverage |
|---|---|---|---|
|---|---|---|---|


## Change Log
| Date | Description |
|---|---|
| 2025-09-17 | Initial Draft |

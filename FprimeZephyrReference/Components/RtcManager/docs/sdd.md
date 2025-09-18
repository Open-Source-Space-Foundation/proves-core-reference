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
| RtcM-001 | The RTC manager shall have a command that calls a port that get time from the RTC Driver | Manual |
| RtcM-002 | An Event is Emitted when time is gotten with the time | Manual |
| RtcM-003 | The RTC manager shall have a command that calls a port that makes the RTC Driver set the time | Manual |

## Port Descriptions
| Name | Description |
|---|---|
| timeSet (output) | Port to reach out to driver to set the time |
| TimeRead (output) | Port to reach out to driver to ask for the time |

## Commands
| Name | Description |
|---|---|
| SET_TIME | Sets the time on the RTC |
| GET_TIME | Gets the time from the RTC |


## Events
| Name | Description |
|---|---|
| GetTime | Event to log the time retrieved from the Rv3028Driver |


## Unit Tests
Add unit test descriptions in the chart below
| Name | Description | Output | Coverage |
|---|---|---|---|
|---|---|---|---|


## Change Log
| Date | Description |
|---|---|
| 2025-09-17 | Initial Draft |
| 2025-09-18 | Update that connects to driver/manager |

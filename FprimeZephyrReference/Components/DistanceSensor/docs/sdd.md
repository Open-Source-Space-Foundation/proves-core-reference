# Components::DistanceSensor

Sensor that detects the distance of the antenna from the satellite. Used by Antenna Deployment component to know if the antennas are deployed yet

## Requirements
Add requirements in the chart below
| Name | Description | Validation |
|DS-001|The distance sensor shall measure its distance from a command input|---|
|DS-002|The distance sensor shall measure its distance from a port input GPIO pin|---|

### Diagrams
Add diagrams here

## Port Descriptions
| Name | Type | Description |
|---|---|----|
|get_data|`Fw::Signal`|Receive signal to send distance information|
|gpioSet|`Drv::GpioWrite`|Control GPIO state to driver|
|schedIn|[`Svc::Sched`]| run | Input | Synchronous | Receive periodic calls from rate group

## Component States
Add component states in the chart below
| Name | Description |
|---|---|

## Sequence Diagrams
Add sequence diagrams here

## Parameters
| Name | Description |
|RngSclFactor| Range scaling factor of the sensoe|
|---|---|

## Commands
| Name | Description |
|---|---|
|GET_DISTANCE|Sends Distance as an event|

## Events
| Name | Description |
|---|---|
|Current_Distance|Sends the current distance sensot|

## Telemetry
| Name | Description |
|---|---|
|Distance|The Distance from the distance sensor|

## Unit Tests
Add unit test descriptions in the chart below
| Name | Description | Output | Coverage |
|---|---|---|---|
|---|---|---|---|


## Change Log
| Date | Description |
|---|---|
|Sep 8| Initial Draft |

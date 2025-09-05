# Components::Burnwire

Driving the Burnwire on and off. The deployment will be handled by the Antenna Deployment component TODO ADD details

## Sequence Diagrams
Add sequence diagrams here

## Requirements
Add requirements in the chart below
| Name | Description | Validation |
| ---- | -----------  | ------ |
|BW-001|The burnwire shall turn on in response to a port call |Hardware Test|
|BW-002|The burnwire shall turn off in response to a port call |Hardware Test|
|BW-003|The burnwire component shall provide an Event when it is turned on and off |Integration Test|
|BW-004|The burnwire component shall activate by turning the GPIO pins on one at a time |Integration Test|
|BW-005|The burnwire component shall be controlled by a safety timeout attached to a 1Hz rate group that can be changed within the code |Integration Test|
|BW-006|The burnwire safety time shall emit an event when it starts and stops |Integration Test|

## Port Descriptions
Name | Type | Description |
|----|---|---|
|burnStop|`Fw::Signal`|Receive stop signal to stop the burnwire|
|burnStart|`Fw::Signal`|Receive start signal to start burnwire|
|gpioSet|`Drv::GpioWrite`|Control GPIO state to driver|
|schedIn|[`Svc::Sched`]| run | Input | Synchronous | Receive periodic calls from rate group


## Commands
| Name | Description |
| ---- | -----------  |
|START_BURNWIRE|Starts the Burn|
|STOP_BURNWIRE|Stops the Burn|

## Events
| Name | Description |
|---|---|
|Burnwire_Start|Emitted once the burnwire has started|
|Burnwire_Stop|Emitted once the burnwire has ended|


## Component States
Add component states in the chart below
| Name | Description |
|----|---|
|m_state|Keeps track if the burnwire is on or off|


## Unit Tests
Add unit test descriptions in the chart below
| Name | Description | Output | Coverage |
|TestSafety|Tests Burnwire turns off after X seconds|---|---|
|TestOn|Tests right GPIO pins turn on |---|---|
|TestOn|Tests right GPIO pins turn off, same as |---|---|


## Parameter
| Name | Description |
| m_safetyMaxCount | The maximum amount that the burnwire will burn before stopping itself for safety |

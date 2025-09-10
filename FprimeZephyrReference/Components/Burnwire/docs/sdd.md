# Components::Burnwire

Turns Burnwire on and off

## Requirements
Add requirements in the chart below
| Name | Description | Validation |
|BW-001|The burnwire component shall turn on when commanded to |Hardware Test|
|BW-002|The burnwire component shall turn off when commanded to |Hardware Test|
|BW-003|The burnwire component shall provide an Event when it is turned on and off |Integration Test|
|BW-004|The burnwire component shall activate by turning the GPIO pins on one at a time |Integration Test|
|BW-005|The burnwire component shall be controlled by a safety timeout that can be changed within the code |Integration Test|


## Port Descriptions
| Name | Description |
|---|---|
|Fw::Signal|Receive stop signal to stop and start burnwire|
|Drv::GpioWrite|Control GPIO state to driver|

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

## Sequence Diagrams
Add sequence diagrams here

## Parameters
| Name | Description |
|---|---|
|---|---|





## Telemetry
| Name | Description |
|---|---|
|---|---|

## Unit Tests
Add unit test descriptions in the chart below
| Name | Description | Output | Coverage |
|TestSafety|Tests Burnwire turns off after X seconds|---|---|
|TestOn|Tests right GPIO pins turn on |---|---|
|TestOn|Tests right GPIO pins turn off, same as on |---|---|

## Change Log
| Date | Description |
|---|---|
|---| Initial Draft |

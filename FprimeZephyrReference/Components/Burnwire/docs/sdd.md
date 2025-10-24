# Components::Burnwire

Driving the Burnwire on and off. This component activates the two pins that are required to heat the burnwire resisitor. The burnwire deployment will be handled by the Antenna Deployment, that will call the ports in the burnwire deployment. For testing, the commands to directly call the burnwire have been left in.

Burnwire is agnostic to the ideal safety count, it simply sets it to be whatever the port or command passes onto

## Sequence Diagrams
Add sequence diagrams here

## Requirements
Add requirements in the chart below
| Name | Description | Validation |
| ---- | -----------  | ------ |
|BW-001|The burnwire shall turn on and off in response to a port calls (TBR for antenna deployer component) |Hardware Test|
|BW-002|The burnwire shall turn on and off in response to commands (TBR for testing for now) |Hardware Test|
|BW-003|The burnwire component shall provide an event when it is turned on and off |Integration Test|
|BW-004|The burnwire component shall activate by turning both the GPIO pins that activate the burnwire | Hardware Test|
|BW-005|The burnwire component shall be controlled by a safety timeout attached to a 1Hz rate group |Integration Test|
|BW-006|The safety timeout shall emit an event when it is changes | Integration test|
|BW-007|The burnwire safety time shall emit an event when it starts and stops |Integration Test|

## Port Descriptions
Name | Type | Description |
|----|---|---|
|burnStop|`Fw::Signal`|Receive stop signal to stop the burnwire|
|burnStart|`Fw::Signal`|Receive start signal to start burnwire|
|gpioSet|`Drv::GpioWrite`|Control GPIO state to driver|
|schedIn|[`Svc::Sched`]| run | Input | Synchronous | Receive periodic calls from rate group|


## Commands
| Name | Description |
| ---- | -----------  |
|START_BURNWIRE|Starts the Burn|
|STOP_BURNWIRE|Stops the Burn|

## Events
| Name | Description |
|---|---|
|SetBurnwireState| Emits burnwire state when the burnwire turns on or off|
|SafetyTimerStatus| Emits safety timer state when the Safety Time has stopped or started|
|SafetyTimerState| Emits the amount of time the safety time will run for when it starts|
| BurnwireEndCount| How long the burnwire actually burned for |

## Component States
Add component states in the chart below
| Name | Description |
|----|---|
|m_state|Keeps track if the burnwire is on or off|

##  Tests
Add unit test descriptions in the chart below
| Name | Description | Output | Coverage |
|------|-------------|--------|----------|
|TestSafety|Tests Burnwire turns off after SAFETY_TIMER seconds|Integration|---|
|TestSafety|Tests Burnwire emits correct events after start and stop|Integration|---|


## Parameter
| Name | Description |
| -----|-------------|
|   SAFETY_TIMER   | By Default set in fpp (currently 10) is the max time the burnwire should ever run|

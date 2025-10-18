# Components::LightSensorManager

Component emitting telemetry read from a VEML 6031 light sensor. This component communicates with the VEML 6031 sensor using a Zephyr driver and provides illuminance data. 

## Usage Examples
Add usage examples here

### Diagrams
Add diagrams here

### Typical Usage
And the typical usage of the component here

## Class Diagram
Add a class diagram here

## Port Descriptions
| Name       | Description                                                                     |
|------------|---------------------------------------------------------------------------------|
| loadSwitch | Input port for receiving signal from load switch to activate the light sensor   |
| run        | Input port for receiving ticks from rate group to read sensor data periodically |

## Component States
| Name | Description       |
|------|-------------------|
| OFF  | The sensor is off |
| ON   | The sensor is on  |

## Sequence Diagrams
Add sequence diagrams here

## Parameters
| Name | Description |
|---|---|
|GAIN|Controls the sensor’s sensitivity to light|
|INTEGRATION_TIME|Determines how long the sensor collects line for before taking a measurement|
|DET_THRESHOLD|Threshold to decide if light is detected|
|READ_INTERVAL|How many rate group ticks it takes to read the light sensor once|

## Commands
| Name  | Description             |
|-------|-------------------------|
| RESET | Turns sensor off and on |

## Events
| Name                 | Description                                          |
|----------------------|------------------------------------------------------|
| LightDetected        | Emitted when light is detected                       |
| LightSensorTurnedOn  | Emitted when light sensor is turned on               |
| LightSensorReset     | Emitted when light sensor resets                     |
| LightSensorTurnedOff | Emitted when light sensor is turned off              |
| LightSensorError     | Emitted when light sensor transitions to error state |

## Telemetry
| Name    | Description                                                    |
|---------|----------------------------------------------------------------|
| Reading | VEML6031 sensor data containing ALS and IR sensor values (raw) |
| Status  | Boolean whether data is valid or not                           |

## Unit Tests
Add unit test descriptions in the chart below
| Name | Description | Output | Coverage |
|---|---|---|---|
|---|---|---|---|

## Requirements
Add requirements in the chart below
| Name | Description | Validation |
|------|-------------|-------------|
| VEML6031-001 | The LightSensorManager shall emit ALS (ambient light sensor) and IR (infrared) sensor values in units of lx (lux) | Unit-Test |
| VEML6031-002 | The LightSensorManager shall communicate with the VEML6031 using its Zephyr driver | Unit-Test |
| VEML6031-003 | The LightSensorManager shall validate lux readings | Unit-Test |
| VEML6031-004 | The LightSensorManager shall report if light is detected above a provided threshold | Unit-Test |
| VEML6031-005 | The LightSensorManager shall reset | Unit-Test |

## Change Log
| Date | Description |
|---|---|
|---| Initial Draft |
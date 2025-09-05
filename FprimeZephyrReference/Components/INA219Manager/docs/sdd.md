# Components::INA219Manager

## 1. Introduction
The `Components::INA219Manager` component manages the interface to the INA219 power monitors. It serves as a wrapper for the Zephyr driver for the INA219 by collecting data and emitting it as telemetry which may be used by other components or sent to ground for further analysis.

At this time we will only implement a simple data collection and forwarding system. In the future we may consider adding to this component error and mode handling for the INA219.

### Usage Examples
Add usage examples here

### Diagrams
Add diagrams here

### Typical Usage
And the typical usage of the component here

## 2. Requirements
Add requirements in the chart below
| Requirement | Description | Verification Method | Verified?
----------- | ----------- | ------------------- | ---------
| PWRM-001 |`Components::INA219Manager` component shall activate upon startup. | Inspection | In Progress |
| PWRM-002 |`Components::INA219Manager` component shall communicate using the I2C bus. | Inspection | In Progress |
| PWRM-003 |`Components::INA219Manager` component shall emit `bus_voltage` in units of V (Volts) | Integration Test | In Progress |
| PWRM-004 |`Components::INA219Manager` component shall emit `current` in units of mA (Milliamps) | Integration Test | In Progress |

## Port Descriptions
| Name | Description |
|---|---|
|---|---|

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
|---|---|

## Events
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
|---|---|---|---|
|---|---|---|---|

## Change Log
| Date | Description |
|---|---|
| September 4, 2025 | Initial Draft |

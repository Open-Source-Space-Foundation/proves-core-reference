# Components::AntennaDeployer

Component that deploys the antenna, activates the burnwire, checks the distance sensor


## Requirements
Add requirements in the chart below
| Name | Description | Validation |
|---|---|---|
|AD0001|The Antenna Deployer shall have a wait period attached to the rate group to deploy antenna after quiet time following satellite deployment | Unit Testing|
|AD0002|The Antenna Deployer shall attempt to redeploy the burnwire if the distance sensor senses the antenna is not deployed from a port| Unit Testing|
|AD0003|The Antenna Deployer shall attempt to deploy if the distance sensor provides nonsensical data |Unit Testing|
|AD0004|The Antenna Deployer shall broadcast an event every time it tries to deploy | Unit Testing|
|AD0005|The Antenna Deployer shall broadcast an event when it successfully deploys | Unit Testing|
|AD0006|The Antenna Deployer shall carry a count of the amount of times it has tried to deploy attached to the Telemetry | Unit Testing|


## Usage Examples
Add usage examples here

### Diagrams
Add diagrams here

### Typical Usage
And the typical usage of the component here

## Class Diagram
Add a class diagram here

## Port Descriptions
| Name | Type | Description |
|------|------| ----------- |
|distVal|F32|Port gets the distance from the distance component |
|startDepl|Fw::Signal|Get command to start deployment|

## Component States
Add component states in the chart below
| Name | Description |
|deploy_count|Keeps track of how many deploys happened |

## Sequence Diagrams
Add sequence diagrams here

## Parameters
| Name | Description |
|deployed_threshold|---|
|invalid_threshold_top|---|
|invalid_threshold_bottom|---|


## Commands
| Name | Description |
| ---- | -----------  |
|DEPLOY|Starts deployment procedure|
|DEPLOY_STOP|Stops deployment procedure|


## Events
| Name | Description |
|Deploy_Attempt|Emitted when deploy attempt starts|
|Deploy_Success|Emitted once the antenna has been detected as successfully deployed|
|Deploy_Finish|Emitted once deploy attempts are finished|


## Telemetry
| Name | Description |
|DeployCount|Reports the amount of time the antenna has tried to deploy|

## Unit Tests
Add unit test descriptions in the chart below
| Name | Description | Output | Coverage |
|---|---|---|---|


## Change Log
| Date | Description |
|---|---|
|---| Initial Draft |

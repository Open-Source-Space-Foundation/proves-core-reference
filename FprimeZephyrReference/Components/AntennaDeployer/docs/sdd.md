# Components::AntennaDeployer

Component that deploys the antenna and activates the burnwire


## Requirements
Add requirements in the chart below
| Name | Description | Validation |
|---|---|---|
|AD0001|The Antenna Deployer shall attempt to redeploy the burnwire if the armed parameter is set| Unit Testing|
|AD0002|The antenna deployer shall attempt to deploy
|AD0003|The Antenna Deployer shall broadcast an event every time it tries to deploy | Unit Testing|
|AD0004|The Antenna Deployer shall broadcast an event when it successfully deploys | Unit Testing|
|AD0005|The Antenna Deployer shall carry a count of the amount of times it has tried to deploy attached to the Telemetry | Unit Testing|


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
|schedIn|Svc.Sched|Port receiving calls from the rate group|
|burnStart|Fw.Signal|Port signaling the burnwire component to start heating|
|burnStop|Fw.Signal|Port signaling the burnwire component to stop heating|

## Component States
Add component states in the chart below
| Name | Description |
|deploy_count|Keeps track of how many deploys happened |

## Sequence Diagrams
Add sequence diagrams here

## Parameters
| Name | Type | Default | Description |
|------|------|---------|-------------|
|RETRY_DELAY_SEC|U32|30|Delay (seconds) between burn attempts|
|MAX_DEPLOY_ATTEMPTS|U32|3|Maximum number of burn attempts before giving up|
|BURN_DURATION_SEC|U32|8|Duration (seconds) for which to hold each burn attempt before issuing STOP|
|DEPLOYED_STATE_FILE|string|"//antenna/antenna_deployer.bin"|File path for persistent deployment state (file exists = deployed)|


## Commands
| Name | Description |
| ---- | -----------  |
|DEPLOY|Starts deployment procedure|
|DEPLOY_STOP|Stops deployment procedure|


## Events
| Name | Description |
|------|------------|
|DeployAttempt|Emitted at the start of each deployment attempt|
|DeploySuccess|Emitted when the antenna deployment is considered successful|
|DeployFinish|Emitted when the deployment procedure finishes|


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

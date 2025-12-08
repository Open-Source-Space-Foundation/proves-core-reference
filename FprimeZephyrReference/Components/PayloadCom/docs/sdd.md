Manager for Payload Communiction

## Usage Examples
See CameraManager Component for how to use this!

### Typical Usage
Configure the PayloadCom component to a uart port to allow for sending and receiving messages.

## Port Descriptions
| Name | Description |
|---|---|
|uartForward|Send a messaged over the UART driver|
|bufferReturn|Return buffer to the UART driver so it can be deallocated|
|commandIn|Port from the connected payload handler to receive commands and forward them over UART|
|uartDataIn|Port for receiving data from the UART driver|
|uartDataOut|Port for forwarding received messages to the connected payload handler|


## Events
| Name | Description |
|---|---|
|CommandForwardError|Component failed to send a message over UART|
|CommandForwardSuccess|Component successfully sent a message over UART|
|UartReceived|Component received UART data|
|AckSent|Component Sent Acknowledgement (Used for message protocols)|

## Unit Tests
Add unit test descriptions in the chart below
| Name | Description | Output | Coverage |
|---|---|---|---|
|---|---|---|---|

## Requirements
Add requirements in the chart below
| Name | Description | Validation |
|---|---|---|
|PayloadCom-1|The component must be able to be configured to a specified UART interface|Manual Test|
|PayloadCom-2|The component must be able to send messages over UART |Manual Test|
|PayloadCom-3| The component must be able to receive messages over UART |Manual Test|
|PayloadCom-4| The component must be able to send acknowledgements over UART |Manual Test|


## Change Log
| Date | Description |
|---| --- |
|Dec 7 2025 |Finalized Version|
|---| Initial Draft |

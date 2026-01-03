# Components::SBand

Wrapper for the [RadioLib SX1280 driver](https://github.com/jgromes/RadioLib). This component integrates into the F Prime communication stack.

### Typical Usage

The component should be connected in accordance with the [F Prime communication adapter interface](../../../../lib/fprime/docs/reference/communication-adapter-interface.md).


## Port Descriptions

| Name | Description |
|---|---|
| run | Scheduler port called by rate group to check for received data |
| Svc.Com | Standard communication interface (dataIn, dataOut, dataReturnIn, dataReturnOut, comStatusIn, comStatusOut) |
| Svc.BufferAllocation | Buffer allocation interface (allocate, deallocate) |
| spiSend | SPI communication with the SX1280 radio |
| resetSend | GPIO control for radio module reset |
| txEnable | GPIO control for S-Band TX enable |
| rxEnable | GPIO control for S-Band RX enable |
| getIRQLine | GPIO read for S-Band IRQ line status |

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
| RadioLibFailed | RadioLib call failed with error code (throttled: 2) |
| AllocationFailed | Failed to allocate buffer for received data (throttled: 2) |
| RadioNotConfigured | Radio not configured, operation ignored (throttled: 3) |

## Telemetry
| Name | Description |
|---|---|
| LastRssi | RSSI (Received Signal Strength Indicator) of last received packet in dBm |
| LastSnr | SNR (Signal-to-Noise Ratio) of last received packet in dB |


## Unit Tests

| Name | Description | Output | Coverage |
|---|---|---|---|
|---|---|---|---|

## Requirements
Add requirements in the chart below
| Name | Description | Validation |
|---|---|---|
|---|---|---|

## Change Log
| Date | Description |
|---|---|
|---| Initial Draft |

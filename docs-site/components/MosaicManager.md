# Components::MosaicManager

Passive component that receives gamma ray detector data from the MOSAIC payload over UART and stores it on disk as [F Prime data products](https://fprime.jpl.nasa.gov/latest/docs/user-manual/framework/data-products/).

The MOSAIC payload streams ASCII CSV lines of the form `ADC=<raw>,MV=<millivolts>\n` at 9600 baud, one sample every 100 ms. The manager only listens — it never sends commands to the payload. Power to the payload is controlled separately through the load switch components.

## Design

- Bytes arriving on `dataIn` are accumulated into lines and parsed into `MosaicSample` records (`adc: U16`, `millivolts: U16`).
- Records are serialized into a data product container obtained synchronously from `Svc.DpManager` (`GammaData` container).
- A container is sent to be written to disk when it fills (100 samples) or when a partially filled container is older than 60 seconds (checked on the 1 Hz rate group), on `FLUSH`, or on `STOP_RECORDING`.
- `Svc.DpWriter` writes containers as `/dp/Dp_<id>_<seconds>_<useconds>.fdp` files, which can be downlinked with the existing file downlink chain.

## Usage Examples

Turn on the payload power load switch; MOSAIC begins streaming immediately and the manager records samples by default. Use `STOP_RECORDING`/`START_RECORDING` to gate recording, and `FLUSH` to force a partially filled data product to disk before downlinking.

## Port Descriptions
| Name           | Description                                                    |
|----------------|----------------------------------------------------------------|
| dataIn         | Raw byte stream from the MOSAIC UART driver                    |
| bufferReturn   | Returns receive buffers to the UART driver                     |
| run            | 1 Hz rate group input for telemetry and stale-container flush  |
| productGetOut  | Synchronous data product container request to Svc.DpManager    |
| productSendOut | Sends filled containers to Svc.DpManager                       |

## Commands
| Name            | Description                                                        |
|-----------------|--------------------------------------------------------------------|
| START_RECORDING | Start recording received samples into data products (default)      |
| STOP_RECORDING  | Stop recording; flushes any partially filled data product to disk  |
| FLUSH           | Flush the partially filled data product to disk now                |

## Events
| Name                 | Description                                                  |
|----------------------|--------------------------------------------------------------|
| RecordingStarted     | Recording was started                                        |
| RecordingStopped     | Recording was stopped                                        |
| DataProductSent      | A container was completed and sent to be written to disk     |
| DpMemoryFail         | Failed to acquire a data product buffer (samples dropped)    |
| LineParseError       | A received line could not be parsed as a MOSAIC sample       |
| RecordSerializeError | A serialization error occurred while recording a sample      |
| UartReceiveError     | UART receive reported a bad status                           |

## Telemetry
| Name             | Description                                          |
|------------------|------------------------------------------------------|
| Recording        | Whether samples are being recorded to data products  |
| SamplesRecorded  | Total samples recorded to data products              |
| ProductsSent     | Total containers sent to be written to disk          |
| ParseErrors      | Total lines that failed to parse                     |
| LatestAdc        | Most recent raw ADC reading (0–4095)                 |
| LatestMillivolts | Most recent reading in millivolts                    |

## Requirements
| Name | Description | Validation |
| -----|-------------|------------|
| MosaicManager-001 | The MosaicManager parses `ADC=<raw>,MV=<millivolts>` lines received over UART. | Integration Test |
| MosaicManager-002 | The MosaicManager stores parsed samples on disk as F Prime data products. | Integration Test |
| MosaicManager-003 | The MosaicManager does not send commands to the MOSAIC payload. | Inspection |
| MosaicManager-004 | The MosaicManager flushes partially filled data products on command and on timeout. | Integration Test |

## Change Log
| Date | Description |
|---|---|
| 2026-07-18 | Initial Draft |

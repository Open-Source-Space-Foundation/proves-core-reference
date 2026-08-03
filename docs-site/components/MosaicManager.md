# Components::MosaicManager

Passive component that receives gamma ray detector data from the MOSAIC payload over UART and stores it on disk under `/mosaic` for later downlink.

The MOSAIC payload streams ASCII CSV lines of the form `ADC=<raw>,MV=<millivolts>\n` at 9600 baud, one sample every 100 ms. The manager only listens — it never sends commands to the payload. Power to the payload is controlled separately through the load switch components.

## Design

- Bytes arriving on `dataIn` are accumulated into lines and parsed into raw ADC/millivolts samples.
- The input ports and the commands are **guarded**, not sync. `dataIn` is invoked from the 10 Hz rate group thread, `run` from the 1 Hz rate group thread, and the commands from the command dispatcher thread; all three mutate the open file handle and its counters. Guarding serializes them on the component mutex so a `STOP_RECORDING` or a stale-file flush cannot close the file out from under an in-flight sample write.
- Samples are appended as fixed-size binary records (`seconds: U32` time tag, `adc: U16`, `millivolts: U16`, 8 bytes total) directly to an `Os::File` opened under `/mosaic`.
- A file is closed when it fills (100 samples) or when a partially filled file is older than 60 seconds (checked on the 1 Hz rate group), on `FLUSH`, or on `STOP_RECORDING`. The next sample reopens a new file.
- Files are named `/mosaic/gamma_<seq>.dat` and can be downlinked with the existing file downlink chain (via a ground-commanded file send — there is no automatic catalog/scan).
- `seq` is an in-RAM counter that resets to 0 on every reboot, but the Zephyr `Os::File` delegate truncates on `OPEN_CREATE` regardless of the `NO_OVERWRITE` flag (unimplemented upstream). To avoid silently wiping a not-yet-downlinked file from a previous boot, `ensureFileOpen()` probes forward with `Os::FileSystem::getPathType()` for the first `seq` not already present on disk before opening.

## Usage Examples

Turn on the payload power load switch; MOSAIC begins streaming immediately and the manager records samples by default. Use `STOP_RECORDING`/`START_RECORDING` to gate recording, and `FLUSH` to force the partially filled file closed before downlinking.

## Port Descriptions
| Name         | Description                                               |
|--------------|-------------------------------------------------------------|
| dataIn       | Raw byte stream from the MOSAIC UART driver                |
| bufferReturn | Returns receive buffers to the UART driver                 |
| run          | 1 Hz rate group input for telemetry and stale-file flush    |

## Commands
| Name            | Description                                                |
|-----------------|-------------------------------------------------------------|
| START_RECORDING | Start recording received samples to the filesystem (default)|
| STOP_RECORDING  | Stop recording; flushes and closes the current file          |
| FLUSH           | Flush and close the current file now                        |

## Events
| Name              | Description                                              |
|-------------------|-----------------------------------------------------------|
| RecordingStarted  | Recording was started                                     |
| RecordingStopped  | Recording was stopped                                     |
| SampleFileClosed  | A sample file was completed and closed                    |
| FileOpenError     | Failed to open a new sample file (samples dropped)         |
| FileWriteError    | A write to the current sample file failed                 |
| LineParseError    | A received line could not be parsed as a MOSAIC sample     |
| UartReceiveError  | UART receive reported a bad status                         |

## Telemetry
| Name             | Description                                          |
|------------------|------------------------------------------------------|
| Recording        | Whether samples are being recorded to the filesystem |
| SamplesRecorded  | Total samples recorded to the filesystem             |
| FilesWritten     | Total sample files closed and ready for downlink     |
| ParseErrors      | Total lines that failed to parse                     |
| LatestAdc        | Most recent raw ADC reading (0–4095)                 |
| LatestMillivolts | Most recent reading in millivolts                    |

## Requirements
| Name | Description | Validation |
| -----|-------------|------------|
| MosaicManager-001 | The MosaicManager parses `ADC=<raw>,MV=<millivolts>` lines received over UART. | Integration Test |
| MosaicManager-002 | The MosaicManager stores parsed samples on disk under /mosaic for later downlink. | Integration Test |
| MosaicManager-003 | The MosaicManager does not send commands to the MOSAIC payload. | Inspection |
| MosaicManager-004 | The MosaicManager flushes and closes the current file on command and on timeout. | Integration Test |

## Change Log
| Date | Description |
|---|---|
| 2026-07-18 | Initial Draft |
| 2026-07-19 | Replaced F Prime data products with direct filesystem writes under /mosaic (DataProducts catalog did not fit in memory on rp2350) |
| 2026-07-19 | Fixed cross-reboot file overwrite: probe for an unused file index before opening, since the Zephyr file delegate ignores NO_OVERWRITE |
| 2026-08-03 | Made the input ports and commands guarded; they run on three different threads and raced on the open file handle |

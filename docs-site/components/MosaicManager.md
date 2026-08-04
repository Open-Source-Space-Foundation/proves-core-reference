# Components::MosaicManager

Passive component that receives gamma ray detector data from the MOSAIC payload over UART and stores it on disk under `/mosaic` for later downlink.

The MOSAIC payload streams ASCII CSV lines of the form `ADC=<raw>,MV=<millivolts>\n` at 9600 baud, one sample every 100 ms. The manager only listens — it never sends commands to the payload. Power to the payload is controlled separately through the load switch components.

## Design

- Bytes arriving on `dataIn` are accumulated into lines and parsed into raw ADC/millivolts samples.
- The input ports and the commands are **guarded**, not sync. `dataIn` is invoked from the 10 Hz rate group thread, `run` from the 1 Hz rate group thread, and the commands from the command dispatcher thread; all three mutate the open file handle and its counters. Guarding serializes them on the component mutex so a `STOP_RECORDING` or a stale-file flush cannot close the file out from under an in-flight sample write.
- Samples are serialized as fixed-size little-endian binary records (`seconds: U32` time tag, `adc: U16`, `millivolts: U16`, 8 bytes total). `SAMPLES_PER_WRITE` records (10 by default) are buffered in memory and appended to the `Os::File` under `/mosaic` in one write; closing a partial file writes the remaining records first.
- A file is closed when it reaches `SAMPLES_PER_FILE` records (100 by default), when a partially filled file is older than 60 seconds (checked on the 1 Hz rate group), on `FLUSH`, or on `STOP_RECORDING`. The next sample reopens a new file.
- Files are named `/mosaic/gamma_<seq>.dat` and can be downlinked with the existing file downlink chain (via a ground-commanded file send — there is no automatic catalog/scan).
- `seq` is an in-RAM counter that resets to 0 on every reboot, but the Zephyr `Os::File` delegate truncates on `OPEN_CREATE` regardless of the `NO_OVERWRITE` flag (unimplemented upstream). To avoid silently wiping a not-yet-downlinked file from a previous boot, `ensureFileOpen()` probes forward with `Os::FileSystem::getPathType()` for the first `seq` not already present on disk before opening. When `MAX_FILE_COUNT` files (40 by default) are present, the manager emits `MaxFilesReached` and disables recording until `START_RECORDING` is commanded.
- File open and write failures increment `FilesystemErrors`. When the count reaches `MAX_FILESYSTEM_ERRORS`, the manager emits `ErrorLimitReached` and disables recording. `START_RECORDING` resets the counter before recording resumes.

## Usage Examples

Turn on the payload power load switch; MOSAIC begins streaming immediately, but the manager does not record samples until `START_RECORDING` is commanded. Use `STOP_RECORDING` to stop and close the current file, and `FLUSH` to force a partially filled file closed before downlinking.

After downlinking a `gamma_*.dat` file, decode and display it from the repository root:

```bash
python3 tools/decode_mosaic.py gamma_000000.dat
```

Pass `--utc` to also render the stored seconds as a UTC timestamp, or `--csv` for output that can be redirected into a spreadsheet. The seconds field is Unix time when the spacecraft RTC is available; if the RTC is unavailable, flight software falls back to seconds since boot.

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

## Parameters
| Name                  | Description                                                        | Default |
|-----------------------|--------------------------------------------------------------------|---------|
| SAMPLES_PER_FILE      | Maximum records stored in one file; zero is treated as one         | 100     |
| SAMPLES_PER_WRITE     | Records buffered into one write; zero is treated as one (U8 range) | 10      |
| MAX_FILE_COUNT        | Maximum sample files allowed; zero disables creation of new files  | 40      |
| MAX_FILESYSTEM_ERRORS | Filesystem errors allowed before recording stops                   | 5       |

## Events
| Name               | Description                                                   |
|--------------------|---------------------------------------------------------------|
| RecordingStarted   | Recording was started                                         |
| RecordingStopped   | Recording was stopped                                         |
| SampleFileClosed   | A sample file was completed and closed                        |
| FileOpenError      | Failed to open a new sample file (samples dropped)             |
| MaxFilesReached    | Maximum file count reached; recording was stopped              |
| FileOperationError | Directory creation or a sample file write failed                 |
| ErrorLimitReached  | Filesystem error limit reached; recording was stopped          |
| LineParseError     | A received line could not be parsed as a MOSAIC sample         |
| UartReceiveError   | UART receive reported a bad status                             |

## Telemetry
| Name             | Description                                          |
|------------------|------------------------------------------------------|
| Recording        | Whether samples are being recorded to the filesystem |
| SamplesRecorded  | Total samples recorded to the filesystem             |
| FilesWritten     | Total sample files closed and ready for downlink     |
| ParseErrors      | Total lines that failed to parse                     |
| FilesystemErrors | Filesystem errors in the current recording run        |
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
| 2026-08-04 | Batched 10 samples per filesystem write to reduce the write rate from 10 Hz to approximately 1 Hz |
| 2026-08-04 | Made per-file sample count, batch size, and maximum file count configurable parameters |
| 2026-08-04 | Added a dedicated maximum-file event and automatic recording stop at the file limit |
| 2026-08-04 | Added filesystem error telemetry and automatic recording stop at a configurable error limit |
| 2026-08-04 | Changed the boot default so recording remains disabled until commanded |

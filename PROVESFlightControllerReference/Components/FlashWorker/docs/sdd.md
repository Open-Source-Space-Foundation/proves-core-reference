# Components::FlashWorker

Performs long-running operations for the flash subsystem. The flash worker is responsible for handling the actual operations needed for flight-software update specific to the Zephyr flash API.

It plays two roles:

1. **Update worker.** It implements the `Update.UpdateWorker` interface, so the generic `Update.Updater` component drives it through prepare, write, next-boot, and confirm.
2. **Image builder.** It owns two commands that prepare a candidate image on the filesystem before that image is written to flash, so that a full image never has to cross the radio link in one piece.

## Why images are not simply uplinked whole

A signed flight image is roughly 727 KB. The ground station paces file uplink at 204-byte chunks with a 0.4 s cooldown (`file-uplink-chunk-size` and `file-uplink-cooldown` in `fprime-gds.yml`), which is about 510 B/s, so a whole image needs roughly 24 minutes of continuous contact. A pass is single-digit minutes, and F Prime's file uplink has no cross-pass resume, so an interrupted transfer loses everything sent so far.

Two ways out, both supported here:

| approach | bytes to uplink | time at 510 B/s |
|---|---|---|
| whole image | 726,784 | ~24 min |
| image in numbered segments | same total, split across passes | survives a pass boundary |
| delta patch against the running image | ~47-97 KB compressed | ~1.5-3.2 min |

Delta figures are measured with bsdiff against real consecutive CI builds. Naive block-level diffing does **not** work: any code size change shifts every later address, so ~99% of 512-byte blocks differ between builds days apart.

## Typical Usage

### Whole image, one pass

```
uplink /update/zephyr.signed.bin
Update.updater.PREPARE_UPDATE
Update.updater.UPDATE_IMAGE_FROM("/update/zephyr.signed.bin", <crc>)
Update.updater.CONFIGURE_NEXT_BOOT(TEST)
reboot
Update.updater.CONFIRM_UPDATE
```

Get `<crc>` from `tools/bin/calculate-crc.py`.

### Image split across several passes

```
uplink /update/img.000, /update/img.001, ...      (one or more per pass)
Update.worker.ASSEMBLE_IMAGE("/update/img", <n>, "/update/candidate.bin", <crc>)
Update.updater.PREPARE_UPDATE
Update.updater.UPDATE_IMAGE_FROM("/update/candidate.bin", <crc>)
```

Assembly verifies the joined image against `<crc>` before anything is written to flash, so a missing or reordered segment is caught rather than left for the bootloader to find.

### Delta patch

```
(ground) tools/bin/make-patch.py <reference.bin> <target.bin> -o update.patch
uplink /update/update.patch
Update.worker.APPLY_PATCH("/update/update.patch", "/update/candidate.bin", <crc>)
Update.updater.PREPARE_UPDATE
Update.updater.UPDATE_IMAGE_FROM("/update/candidate.bin", <crc>)
```

The reference is the running image, read directly out of the `slot0_partition` flash area. The RP2350 executes XIP from memory-mapped QSPI, so no copy has to be kept on the filesystem. The patch container records the size and CRC32 of the reference it was built from, and `APPLY_PATCH` refuses to run if the running image is not that one. Patching the wrong reference produces a plausible but corrupt image that would then be flashed and booted, so this check is not optional.

**Compression is not yet available on the flight side.** The patch streams are ~84% zero bytes and compress from ~727 KB to ~60 KB with DEFLATE, ~47 KB with LZMA, or ~97 KB with heatshrink, but this Zephyr workspace ships no decompressor and selecting that dependency is a project decision. Until it is made, `make-patch.py` refuses to emit a patch without `--allow-uncompressed`, because an uncompressed patch is the size of the image and worth nothing over the radio. The container carries a codec field so a compressed format can be added without changing the applier's structure.

## Flash Layout

Defined in `boards/bronco_space/proves_flight_control_board_v5/proves_flight_control_board_v5.dtsi`.

| partition | label | size | role |
|---|---|---|---|
| `boot_partition` | mcuboot | 1 MB | bootloader |
| `slot0_partition` | primary | 1 MB | running image, and the patch reference |
| `slot1_partition` | secondary | 1 MB | staging slot updates are written into |
| `slot2_partition` | reserved | 1 MB | unused in swap-using-offset mode with one image |
| `storage_partition` | n/a | 12 MB | LittleFS, holds uplinked segments and candidates |

Partition IDs are taken from the device tree with `FIXED_PARTITION_ID`, never hard coded: fixed partition IDs follow declaration order, so a literal would silently point at the wrong region if a partition were added above it.

## Port Descriptions
| Name | Description |
|---|---|
| prepareImage | Erase the staging slot, from `Update.Updater` |
| updateImage | Write an image file into the staging slot |
| nextBoot | Set the next boot mode through MCUBoot |
| confirmImage | Confirm the running image so it is not reverted |
| prepareImageDone / updateImageDone | Report completion of the slow operations |

## Component States

The sequence is tracked by `Components::UpdateSequencer`.

| Name | Description |
|---|---|
| IDLE | No usable staging slot; PREPARE_UPDATE must run before an update |
| PREPARED | Staging slot erased and ready to receive an image |
| UPDATED | An image has been written to the staging slot |

A failure that never reached the flash (a bad file name, a failed size or CRC read, a CRC mismatch) leaves the sequence in PREPARED, so the operator can retry without paying for another 1 MB erase. A failure that did reach the flash drops to IDLE, because the slot now holds partial data and must be erased again.

## Parameters
| Name | Description |
|---|---|
| CHUNK_DELAY_US | Microseconds to pause after each buffered flash write, default 5000. Exposed so it can be tuned against real hardware instead of rebuilt; at the default a 727 KB image spends about 7 s asleep. |
| PROGRESS_STEP_PERCENT | Percent of the image between progress events, default 10. Larger values spend less downlink reporting on an update in flight. |

## Commands
| Name | Description |
|---|---|
| ASSEMBLE_IMAGE | Concatenate numbered uplink segments into one image file and verify its CRC32 |
| APPLY_PATCH | Reconstruct an image from a delta patch applied to the running image |

## Events
| Name | Description |
|---|---|
| UpdateProgress | Periodic progress during a write |
| NoImagePrepared | An update was requested before a successful preparation |
| NextBootSetFailed / ConfirmImageFailed | MCUBoot next-boot or confirm call failed |
| FlashEraseFailed / FlashWriteFailed | Staging slot erase or write failed |
| ImageFileReadError / ImageFileCrcMismatch | Image file could not be read, or failed validation |
| ImageWriteCrcMismatch | Bytes written to flash did not match the bytes validated |
| AssembleStarted / AssembleSucceeded / AssembleFailed / InvalidSegmentCount | Segment assembly |
| PatchStarted / PatchSucceeded / PatchFailed / PatchReferenceMismatch | Patch application |

## Telemetry
| Name | Description |
|---|---|
| UpdateStage | IDLE, PREPARING, PREPARED, WRITING, UPDATED, or FAILED |
| BytesWritten | Bytes of the image written into the staging slot so far |
| ImageTotalBytes | Total size of the image being written |
| LastUpdateStatus | Status of the most recent preparation or update |

These are channels rather than only events so that an operator returning on a later pass can ask where an update stands without replaying event history. They are packetized in `SoftwareUpdate` (packet 23, group 5).

## Unit Tests

Host tests, no F Prime or Zephyr dependency. Run with `make test-unit`.

| Name | Description | Output | Coverage |
|---|---|---|---|
| test_FlashWorker_UpdateSequencer | Sequence ordering, status mapping, retry cost, progress arithmetic | pass/fail | `UpdateSequencer` |
| test_FlashWorker_SegmentPlan | Segment naming, zero padding, buffer and index bounds | pass/fail | `SegmentPlan` |
| test_FlashWorker_PatchApplier | Container decoding, patch application, malformed and hostile patches | pass/fail | `PatchApplier` |

Integration tests covering the command surface against hardware are in `test/int/ota_test.py`. They deliberately never set the next boot, so a run cannot leave the board staged to boot an unintended image.

## Requirements

| Name | Description | Validation |
|---|---|---|
| A failed update is never reported as a success | Read and write failures propagate a failure status to the Updater | Unit test |
| A retry costs an erase only when one is needed | Failures that never reached flash leave the slot usable | Unit test, integration test |
| An image is validated before it is flashed | CRC32 is checked before the write, and the written bytes are verified after | Unit test, inspection |
| A patch is applied only to the image it was built from | The container records the reference size and CRC and both are checked | Unit test |
| A malformed patch cannot read or write out of bounds | Control records are range checked against both images | Unit test |

## Change Log
| Date | Description |
|---|---|
| n/a | Initial Draft |
| 2026-08-04 | Correct failure reporting, add progress telemetry and parameters, add segment assembly and delta patching |

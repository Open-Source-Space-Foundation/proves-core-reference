# Components::FlashWorker

Performs long-running operations for the flash subsystem. The flash worker is responsible for handling the actual operations needed for flight-software update specific to the Zephyr flash API.

## Update region

Every flash operation here targets `REGION_NUMBER`, the MCUboot **secondary
slot** — the staging area an uploaded image is written to before the bootloader
swaps it into the primary slot. It is resolved at compile time from the
devicetree label:

```cpp
constexpr static U8 REGION_NUMBER = PARTITION_ID(slot1_partition);
```

**Never replace this with a literal number.** Zephyr assigns flash-area IDs by
devicetree *dependency ordinal*, not by address or declaration order, so adding
a partition anywhere in the devicetree renumbers every area. A hardcoded ID
therefore starts silently pointing at a different partition — and when that
partition is `slot0_partition`, `prepareImage` erases the firmware that is
currently executing and the board is bricked until it is reflashed over UF2 or
SWD. A `static_assert` in `FlashWorker.hpp` fails the build if the update region
ever resolves to the running code partition, and
`PROVESFlightControllerReference/test/int/ota_test.py` covers it on hardware.


## Usage Examples
Add usage examples here

### Diagrams
Add diagrams here

### Typical Usage
And the typical usage of the component here

## Class Diagram
Add a class diagram here

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

## Requirements
Add requirements in the chart below
| Name | Description | Validation |
|---|---|---|
|---|---|---|

## Change Log
| Date | Description |
|---|---|
|---| Initial Draft |

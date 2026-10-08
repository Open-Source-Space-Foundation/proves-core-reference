# PROVES Core Reference Tools

This directory contains development and analysis tools for the PROVES Core Reference project.

## Data Budget Tool

The Data Budget Tool (`data_budget.py`) analyzes F Prime telemetry definitions to calculate the serialized byte size of telemetry channels and packets.

### Features

- **Automatic FPP Parsing**: Parses all `.fpp` and `.fppi` files in the `PROVESFlightControllerReference` directory to extract telemetry channel definitions
- **Type Size Calculation**: Calculates serialized sizes for primitive types, structs, enums, and arrays
- **Packet Analysis**: Analyzes telemetry packets to show total byte usage per packet
- **Group Summary**: Aggregates packet sizes by telemetry group
- **Compact Output**: By default shows only summary and group totals; use verbose mode for full details
- **Unresolved Channel Tracking**: Reports channels that couldn't be resolved (e.g., from F Prime framework components)
- **Mermaid Wire Diagrams**: Generates Mermaid `packet-beta` diagrams showing the full byte layout of each packet on the wire, rendered natively on GitHub, GitLab, and most modern Markdown viewers

### Usage

#### Quick Analysis (Summary Only)

```bash
make data-budget
```

This displays a summary showing:
- Total telemetry channels and bytes
- Bytes per telemetry group

#### Detailed Analysis (Verbose Mode)

```bash
make data-budget VERBOSE=1
```

This displays the full report including:
- Telemetry channels by component
- Telemetry packet sizes
- Detailed packet composition

#### Mermaid Packet Wire Diagrams

```bash
# Print Markdown with embedded Mermaid diagrams to stdout
make data-diagram

# Save to a Markdown file
make data-diagram OUTPUT=docs-site/telemetry-wire-diagrams.md
```

Each packet in the output gets:
1. A **Mermaid `packet-beta` diagram** showing the full wire layout — CCSDS TM framing headers followed by every telemetry field at its exact byte offset. The diagram renders as a protocol-style bit/byte box layout (like RFC diagrams) in any Mermaid-aware renderer.
2. A **companion Markdown table** listing each field's byte offset, F Prime type, size, and the full F Prime channel path — useful for copy-paste or programmatic processing.

#### Direct Script Usage

```bash
# Summary output (default)
python3 tools/data_budget.py

# Detailed output
python3 tools/data_budget.py --verbose

# Mermaid packet diagrams (printed to stdout)
python3 tools/data_budget.py --diagram

# Mermaid packet diagrams saved to a file
python3 tools/data_budget.py --diagram --output telemetry-wire-diagrams.md

# Specify project root
python3 tools/data_budget.py --project-root /path/to/project
```

### Diagram Output Format

The Mermaid `packet-beta` diagram uses **bit-based indices** (the Mermaid standard) with 32 bits (4 bytes) displayed per row. Each row therefore represents 4 bytes of wire data, making it easy to visualise byte alignment.

**Framing overhead prepended to every packet:**

| Field | Size |
|---|---|
| CCSDS TM Frame Header | 6 B |
| CCSDS Space Packet Header | 6 B |
| F Prime Packet Descriptor (U32) | 4 B |
| F Prime Packet ID (U16) | 2 B |
| **Total overhead** | **18 B** |

**Example diagram block (truncated):**

````markdown
```mermaid
packet-beta
title Beacon — ID 1, Group 1 (Beacon)
0-47: "TM Frame Header (6B)"
48-95: "Space Packet Header (6B)"
96-127: "FPrime Descriptor (4B)"
128-143: "Pkt ID (2B)"
144-207: "BootCount (8B)"
208-215: "CurrentMode (1B)"
...
```
````

### Understanding the Output

#### Summary Section (Always Shown)

```text
SUMMARY
--------------------------------------------------------------------------------
Total Telemetry Channels: 83
Channels with Known Size: 83
Total Telemetry Data: 607 bytes
Total Telemetry Packets: 21
```

- **Total Telemetry Channels**: Number of telemetry channels defined across all components
- **Channels with Known Size**: How many channels have calculable sizes (should be 100%)
- **Total Telemetry Data**: Sum of all telemetry channel sizes
- **Total Telemetry Packets**: Number of telemetry packets defined

#### Bytes Per Telemetry Group (Always Shown)

Shows a summary of data usage by telemetry group:
- **Group**: Group ID number (1-6)
- **Description**: Purpose of the group (Beacon, Live Satellite Sensor Data, etc.)
- **Packets**: Number of packets in the group
- **Total Size (bytes)**: Sum of all packet sizes in the group

Telemetry groups organize packets by function:
1. **Beacon** - Essential health data transmitted frequently
2. **Live Satellite Sensor Data** - Real-time sensor readings
3. **Satellite Meta Data** - Configuration and state information
4. **Payload Meta Data** - Payload-specific information
5. **Health and Status** - System health monitoring data
6. **Parameters** - Configuration parameters

#### Telemetry Channels Table (Verbose Mode Only)

Shows each channel with:
- **Component**: The F Prime component that defines the channel
- **Channel**: The telemetry channel name
- **Type**: The F Prime type (F32, U32, custom structs, etc.)
- **Size (bytes)**: Serialized size in bytes

#### Telemetry Packets Table (Verbose Mode Only)

Shows each packet with:
- **Packet Name**: Name from the packet definition
- **ID**: Packet ID number
- **Group**: Packet group number
- **Channels**: Number of channels in the packet
- **Total Size (bytes)**: Sum of all channel sizes in the packet

#### Detailed Packet Breakdown (Verbose Mode Only)

For each packet, shows:
- All channels included in the packet
- The full path to each channel (e.g., `ReferenceDeployment.imuManager.Acceleration`)
- The type and size of each channel

### Type Sizes

The tool recognizes F Prime primitive types and their serialized sizes:

| Type | Size (bytes) | Description |
|------|--------------|-------------|
| U8, I8, bool | 1 | 8-bit unsigned/signed integer, boolean |
| U16, I16 | 2 | 16-bit unsigned/signed integer |
| U32, I32, F32 | 4 | 32-bit unsigned/signed integer, float |
| U64, I64, F64 | 8 | 64-bit unsigned/signed integer, double |
| Enums | 4 | Serialized as I32 |
| Fw.TimeValue | 12 | Timestamp struct (seconds + microseconds + timeBase) |
| Fw.TimeIntervalValue | 12 | Time interval struct |
| FwSizeType | 8 | File size type (U64) |

Custom structs are calculated as the sum of their fields. For example:
- `Drv.Acceleration` (3× F64) = 24 bytes
- `Drv.MagneticField` (3× F64 + Fw.TimeValue) = 36 bytes

### Example Output

#### Default (Summary) Mode

```
SUMMARY
--------------------------------------------------------------------------------
Total Telemetry Channels: 83
Channels with Known Size: 83
Total Telemetry Data: 607 bytes
Total Telemetry Packets: 21

BYTES PER TELEMETRY GROUP
--------------------------------------------------------------------------------
Group    Description                         Packets    Total Size (bytes)
--------------------------------------------------------------------------------
1        Beacon                              1          65
2        Live Satellite Sensor Data          4          168
3        Satellite Meta Data                 3          120
4        Payload Meta Data                   1          18
5        Health and Status                   6          49
6        Parameters                          6          296
--------------------------------------------------------------------------------
TOTAL                                                   716
```

#### Verbose Mode (Excerpt)

```
Packet: Beacon (ID: 1, Group: 1)
  Total Size: 65 bytes
  Channels (10):
    - ReferenceDeployment.startupManager.BootCount           FwSizeType           8 bytes
    - ReferenceDeployment.modeManager.CurrentMode            U8                   1 bytes
    - ReferenceDeployment.imuManager.AngularVelocity         Drv.AngularVelocity  24 bytes
    - ReferenceDeployment.ina219SysManager.Voltage           F64                  8 bytes
    ...
```

### Troubleshooting

**UNKNOWN type sizes**: If you see "UNKNOWN" for type sizes in verbose mode, the tool couldn't find the type definition. Possible causes:
- Type is defined in a file not being parsed
- Type is from an external F Prime module not included
- Type name doesn't match the definition

**Missing instances**: If packet channels show "UNKNOWN", the instance might be defined in a subtopology or external file. Check that all relevant `.fpp` files are in the search path.

**Diagram fields showing `(?)`**: A channel reference in a `.fppi` packet file couldn't be resolved to a known channel. The field is omitted from the Mermaid diagram but still appears in the companion table so the gap is visible.

### Use Cases

1. **Quick Budget Check**: Use default mode to get a fast overview of total bytes per telemetry group
2. **Downlink Budget Planning**: Calculate maximum downlink data size for mission planning
3. **Bandwidth Analysis**: Identify which groups and packets consume the most bandwidth
4. **Optimization**: Find opportunities to reduce telemetry data size
5. **Documentation**: Generate telemetry data specifications and wire-format diagrams for mission documentation

### Implementation Details

The tool:
1. Scans `PROVESFlightControllerReference/Components/` and `PROVESFlightControllerReference/ReferenceDeployment/` for FPP files
2. Parses type definitions (structs, enums, arrays)
3. Extracts telemetry channel definitions
4. Maps component instances to their types
5. Parses telemetry packet definitions from `.fppi` files
6. Calculates sizes based on F Prime serialization rules
7. Groups packets by telemetry group and calculates group totals
8. Generates summary or detailed reports based on verbose flag
9. Optionally generates Mermaid `packet-beta` wire diagrams with companion tables

### Future Enhancements

Potential improvements:
- Support for parameterized array sizes
- Command argument size calculations
- Event size calculations
- Comparison between builds to track changes
- Integration with F Prime dictionary files for validation

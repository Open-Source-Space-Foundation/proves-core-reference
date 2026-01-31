# PROVES Core Reference Tools

This directory contains development and analysis tools for the PROVES Core Reference project.

## Data Budget Tool

The Data Budget Tool (`data_budget.py`) analyzes F Prime telemetry definitions to calculate the serialized byte size of telemetry channels and packets.

### Features

- **Automatic FPP Parsing**: Parses all `.fpp` and `.fppi` files to extract telemetry channel definitions
- **Type Size Calculation**: Calculates serialized sizes for primitive types, structs, enums, and arrays
- **Packet Analysis**: Analyzes telemetry packets to show total byte usage per packet
- **Multiple Output Formats**: Supports text reports and JSON output for programmatic consumption

### Usage

#### Quick Analysis (Console Output)

```bash
make data-budget
```

This displays a comprehensive report showing:
- Total telemetry channels and their sizes
- Breakdown by component
- Telemetry packet sizes
- Detailed packet composition

#### Generate Report File

```bash
make data-budget-report
```

Generates `data_budget_report.txt` with the full analysis.

#### Generate JSON Report

```bash
make data-budget-json
```

Generates `data_budget.json` with structured data for programmatic use.

#### Direct Script Usage

```bash
# Console output
python3 tools/data_budget.py

# Save to file
python3 tools/data_budget.py --output my_report.txt

# Generate JSON
python3 tools/data_budget.py --json my_data.json

# Specify project root
python3 tools/data_budget.py --project-root /path/to/project
```

### Understanding the Output

#### Summary Section

```
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

#### Telemetry Channels Table

Shows each channel with:
- **Component**: The F Prime component that defines the channel
- **Channel**: The telemetry channel name
- **Type**: The F Prime type (F32, U32, custom structs, etc.)
- **Size (bytes)**: Serialized size in bytes

#### Telemetry Packets Table

Shows each packet with:
- **Packet Name**: Name from the packet definition
- **ID**: Packet ID number
- **Group**: Packet group number
- **Channels**: Number of channels in the packet
- **Total Size (bytes)**: Sum of all channel sizes in the packet

#### Detailed Packet Breakdown

For each packet, shows:
- All channels included in the packet
- The full path to each channel (e.g., `ReferenceDeployment.imuManager.Acceleration`)
- The type and size of each channel

#### Bytes Per Telemetry Group

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

This shows the Beacon packet contains 10 channels totaling 65 bytes when serialized.

### Troubleshooting

**UNKNOWN type sizes**: If you see "UNKNOWN" for type sizes, the tool couldn't find the type definition. Possible causes:
- Type is defined in a file not being parsed
- Type is from an external F Prime module not included
- Type name doesn't match the definition

**Missing instances**: If packet channels show "UNKNOWN", the instance might be defined in a subtopology or external file. Check that all relevant `.fpp` files are in the search path.

### Use Cases

1. **Downlink Budget Planning**: Calculate maximum downlink data size for mission planning
2. **Bandwidth Analysis**: Identify which packets consume the most bandwidth
3. **Optimization**: Find opportunities to reduce telemetry data size
4. **Documentation**: Generate telemetry data specifications for mission documentation
5. **CI/CD Integration**: Include in automated builds to track data budget over time

### Implementation Details

The tool:
1. Scans `FprimeZephyrReference/Components/` and `FprimeZephyrReference/ReferenceDeployment/` for FPP files
2. Parses type definitions (structs, enums, arrays)
3. Extracts telemetry channel definitions
4. Maps component instances to their types
5. Parses telemetry packet definitions from `.fppi` files
6. Calculates sizes based on F Prime serialization rules
7. Generates reports

### Future Enhancements

Potential improvements:
- Support for parameterized array sizes
- Command argument size calculations
- Event size calculations
- Comparison between builds to track changes
- Integration with F Prime dictionary files for validation

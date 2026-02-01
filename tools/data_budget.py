#!/usr/bin/env python3
"""
Data Budget Tool for F Prime Telemetry

This tool analyzes FPP files to calculate the serialized byte size of telemetry channels.
It parses component definitions, type definitions, and telemetry packet specifications
to provide a comprehensive data budget report.

Usage:
    python3 tools/data_budget.py [--verbose]
"""

import argparse
import re
import sys
from dataclasses import dataclass, field
from pathlib import Path
from typing import Dict, List, Optional

# F Prime primitive type sizes in bytes (as serialized)
PRIMITIVE_TYPE_SIZES = {
    "U8": 1,
    "I8": 1,
    "U16": 2,
    "I16": 2,
    "U32": 4,
    "I32": 4,
    "U64": 8,
    "I64": 8,
    "F32": 4,
    "F64": 8,
    "bool": 1,
    "NATIVE_INT_TYPE": 4,  # Platform-dependent, assuming 32-bit
    "NATIVE_UINT_TYPE": 4,
}


@dataclass
class TypeDefinition:
    """Represents a type definition (struct, enum, or primitive)"""

    name: str
    kind: str  # 'struct', 'enum', 'primitive', 'array'
    size_bytes: Optional[int] = None
    fields: Dict[str, str] = field(default_factory=dict)  # field_name -> type_name
    module: Optional[str] = None


@dataclass
class ComponentInstance:
    """Represents a component instance"""

    instance_name: str
    component_type: str


@dataclass
class TelemetryChannel:
    """Represents a telemetry channel definition"""

    name: str
    type_name: str
    component: str
    size_bytes: Optional[int] = None
    module: Optional[str] = None


@dataclass
class TelemetryPacket:
    """Represents a telemetry packet definition"""

    name: str
    packet_id: int
    group_id: int
    channels: List[str] = field(default_factory=list)  # List of channel paths
    total_size_bytes: int = 0
    unresolved_channels: List[str] = field(default_factory=list)  # Channels that couldn't be resolved


class DataBudgetAnalyzer:
    """Analyzes FPP files to calculate telemetry data budget"""

    def __init__(self, project_root: Path):
        self.project_root = project_root
        self.types: Dict[str, TypeDefinition] = {}
        self.telemetry_channels: Dict[str, TelemetryChannel] = {}
        self.telemetry_packets: Dict[str, TelemetryPacket] = {}
        self.instances: Dict[
            str, ComponentInstance
        ] = {}  # instance_name -> ComponentInstance
        self.current_module: Optional[str] = None

        # Initialize F Prime primitive types
        for type_name, size in PRIMITIVE_TYPE_SIZES.items():
            self.types[type_name] = TypeDefinition(
                name=type_name, kind="primitive", size_bytes=size
            )

    def find_fpp_files(self) -> List[Path]:
        """Find all FPP files in the project"""
        fpp_files: List[Path] = []

        # Search across the entire FprimeZephyrReference tree so that
        # instances/types defined in other top-level directories (e.g.
        # communications components) are also discovered.
        reference_root = self.project_root / "FprimeZephyrReference"
        if reference_root.exists():
            fpp_files.extend(reference_root.rglob("*.fpp"))
            fpp_files.extend(reference_root.rglob("*.fppi"))

        return fpp_files

    def parse_module(self, line: str) -> Optional[str]:
        """Extract module name from module declaration"""
        match = re.match(r"\s*module\s+(\w+)\s*\{", line)
        if match:
            return match.group(1)
        return None

    def parse_struct_definition(
        self, lines: List[str], start_idx: int
    ) -> Optional[TypeDefinition]:
        """Parse a struct definition from FPP"""
        struct_match = re.match(r"\s*struct\s+(\w+)\s*\{", lines[start_idx])
        if not struct_match:
            return None

        struct_name = struct_match.group(1)
        fields = {}

        # Parse fields until closing brace
        i = start_idx + 1
        while i < len(lines):
            line = lines[i].strip()
            if line.startswith("}"):
                break

            # Parse field: name: Type
            field_match = re.match(r"(\w+)\s*:\s*([A-Za-z0-9_.]+)", line)
            if field_match:
                field_name = field_match.group(1)
                field_type = field_match.group(2)
                fields[field_name] = field_type

            i += 1

        # Calculate struct size
        struct_size = self.calculate_struct_size(fields)

        return TypeDefinition(
            name=struct_name,
            kind="struct",
            size_bytes=struct_size,
            fields=fields,
            module=self.current_module,
        )

    def parse_enum_definition(
        self, lines: List[str], start_idx: int
    ) -> Optional[TypeDefinition]:
        """Parse an enum definition from FPP"""
        enum_match = re.match(r"\s*enum\s+(\w+)\s*\{", lines[start_idx])
        if not enum_match:
            return None

        enum_name = enum_match.group(1)

        # Enums are serialized as I32 (4 bytes) in F Prime
        return TypeDefinition(
            name=enum_name, kind="enum", size_bytes=4, module=self.current_module
        )

    def parse_array_definition(
        self, lines: List[str], start_idx: int
    ) -> Optional[TypeDefinition]:
        """Parse an array definition from FPP"""
        array_match = re.match(
            r"\s*array\s+(\w+)\s*=\s*\[(\d+)\]\s*([A-Za-z0-9_.]+)", lines[start_idx]
        )
        if not array_match:
            return None

        array_name = array_match.group(1)
        array_size = int(array_match.group(2))
        element_type = array_match.group(3)

        # Calculate array size: element_size * count + size field (U16 for size)
        element_size = self.get_type_size(element_type)
        total_size = 2 + (element_size * array_size) if element_size else None

        return TypeDefinition(
            name=array_name,
            kind="array",
            size_bytes=total_size,
            module=self.current_module,
        )

    def parse_telemetry_channel(
        self, line: str, component_name: str
    ) -> Optional[TelemetryChannel]:
        """Parse a telemetry channel definition from FPP"""
        # Match: telemetry ChannelName: TypeName
        tlm_match = re.match(r"\s*telemetry\s+(\w+)\s*:\s*([A-Za-z0-9_.]+)", line)
        if not tlm_match:
            return None

        channel_name = tlm_match.group(1)
        type_name = tlm_match.group(2)

        # Calculate channel size
        size = self.get_type_size(type_name)

        return TelemetryChannel(
            name=channel_name,
            type_name=type_name,
            component=component_name,
            size_bytes=size,
            module=self.current_module,
        )

    def calculate_struct_size(self, fields: Dict[str, str]) -> Optional[int]:
        """Calculate the size of a struct based on its fields"""
        total_size = 0
        for field_type in fields.values():
            field_size = self.get_type_size(field_type)
            if field_size is None:
                return None  # Unknown type size
            total_size += field_size
        return total_size

    def get_type_size(self, type_name: str) -> Optional[int]:
        """Get the size in bytes of a type"""
        # Handle qualified names (e.g., Drv.Acceleration, Fw.TimeValue)
        # Try full name first
        if type_name in self.types:
            return self.types[type_name].size_bytes

        # Try without module prefix
        simple_name = type_name.split(".")[-1]
        if simple_name in self.types:
            return self.types[simple_name].size_bytes

        # Try with current module prefix
        if self.current_module:
            qualified_name = f"{self.current_module}.{simple_name}"
            if qualified_name in self.types:
                return self.types[qualified_name].size_bytes

        # Search in all modules
        for type_key, type_def in self.types.items():
            if type_key.endswith(f".{simple_name}"):
                return type_def.size_bytes

        # Unknown type
        return None

    def parse_component_file(self, fpp_path: Path):
        """Parse a component FPP file for type and telemetry definitions"""
        try:
            with open(fpp_path, "r") as f:
                lines = f.readlines()
        except Exception as e:
            print(f"Warning: Could not read {fpp_path}: {e}", file=sys.stderr)
            return

        # Reset module context for each file
        self.current_module = None

        # Try to extract component name from path or content
        component_name = fpp_path.stem

        i = 0
        while i < len(lines):
            line = lines[i]

            # Track module context
            module_match = self.parse_module(line)
            if module_match:
                self.current_module = module_match

            # Check for module closing - reset module context
            if re.match(r"\s*\}", line) and self.current_module:
                self.current_module = None

            # Parse component declaration to get name
            comp_match = re.match(
                r"\s*(?:passive|active|queued)\s+component\s+(\w+)", line
            )
            if comp_match:
                component_name = comp_match.group(1)

            # Parse instance declarations
            inst_match = re.match(r"\s*instance\s+(\w+)\s*:\s*([A-Za-z0-9_.]+)", line)
            if inst_match:
                instance_name = inst_match.group(1)
                component_type = inst_match.group(2)
                # Remove module prefix to get just the component name
                simple_type = component_type.split(".")[-1]
                self.instances[instance_name] = ComponentInstance(
                    instance_name=instance_name, component_type=simple_type
                )

            # Parse type definitions
            if "struct" in line and "{" in line:
                type_def = self.parse_struct_definition(lines, i)
                if type_def:
                    # Store with module-qualified name
                    key = (
                        f"{self.current_module}.{type_def.name}"
                        if self.current_module
                        else type_def.name
                    )
                    self.types[key] = type_def
                    # Also store simple name for easier lookup
                    self.types[type_def.name] = type_def

            elif "enum" in line and "{" in line:
                type_def = self.parse_enum_definition(lines, i)
                if type_def:
                    key = (
                        f"{self.current_module}.{type_def.name}"
                        if self.current_module
                        else type_def.name
                    )
                    self.types[key] = type_def
                    self.types[type_def.name] = type_def

            elif "array" in line and "=" in line:
                type_def = self.parse_array_definition(lines, i)
                if type_def:
                    key = (
                        f"{self.current_module}.{type_def.name}"
                        if self.current_module
                        else type_def.name
                    )
                    self.types[key] = type_def
                    self.types[type_def.name] = type_def

            # Parse telemetry channels
            elif "telemetry" in line and ":" in line:
                channel = self.parse_telemetry_channel(line, component_name)
                if channel:
                    # Store with component-qualified name
                    key = f"{component_name}.{channel.name}"
                    self.telemetry_channels[key] = channel

            i += 1

    def parse_packet_file(self, fppi_path: Path):
        """Parse a telemetry packet definition file (*.fppi)"""
        try:
            with open(fppi_path, "r") as f:
                lines = f.readlines()
        except Exception as e:
            print(f"Warning: Could not read {fppi_path}: {e}", file=sys.stderr)
            return

        current_packet = None
        i = 0

        while i < len(lines):
            line = lines[i].strip()

            # Parse packet definition: packet Name id X group Y {
            packet_match = re.match(
                r"packet\s+(\w+)\s+id\s+(\d+)\s+group\s+(\d+)\s*\{", line
            )
            if packet_match:
                packet_name = packet_match.group(1)
                packet_id = int(packet_match.group(2))
                group_id = int(packet_match.group(3))

                current_packet = TelemetryPacket(
                    name=packet_name, packet_id=packet_id, group_id=group_id
                )
                self.telemetry_packets[packet_name] = current_packet

            # Parse channel reference in packet
            elif (
                current_packet and not line.startswith("#") and not line.startswith("}")
            ):
                # Channel references can be:
                # - 3-segment: ReferenceDeployment.component.ChannelName
                # - 2-segment: component.ChannelName or instance.ChannelName
                # - May contain $ for special instances like CdhCore.$health.PingLateWarnings
                # Parse by splitting on dots and extracting components
                if line and not line.isspace():
                    # Remove any trailing comments or whitespace
                    clean_line = line.split("#")[0].strip()
                    if clean_line:
                        # Store the full path as-is for later resolution
                        current_packet.channels.append(clean_line)

            # End of packet definition
            if current_packet and line.startswith("}"):
                current_packet = None

            i += 1

    def recalculate_channel_sizes(self):
        """Recalculate channel sizes after all types are loaded"""
        for channel in self.telemetry_channels.values():
            if channel.size_bytes is None:
                channel.size_bytes = self.get_type_size(channel.type_name)

    def calculate_packet_sizes(self):
        """Calculate total size for each telemetry packet"""
        for packet in self.telemetry_packets.values():
            total_size = 0
            packet.unresolved_channels = []
            
            for channel_path in packet.channels:
                resolved = False
                # Try to find the channel
                # Format can be:
                # - 3+ segments: ReferenceDeployment.instanceName.ChannelName
                # - 2 segments: instanceName.ChannelName
                # - May contain special chars like $ in CdhCore.$health.PingLateWarnings
                parts = channel_path.split(".")
                
                if len(parts) >= 3:
                    # Try 3-segment format: deployment.instance.channel
                    instance_name = parts[1]
                    channel_name = parts[2]

                    # Look up the instance to get the component type
                    if instance_name in self.instances:
                        component_type = self.instances[instance_name].component_type
                        component_channel_key = f"{component_type}.{channel_name}"

                        if component_channel_key in self.telemetry_channels:
                            channel = self.telemetry_channels[component_channel_key]
                            if channel.size_bytes is not None:
                                total_size += channel.size_bytes
                                resolved = True
                    
                    if not resolved:
                        # Try direct match (for instances from other deployments/subtopologies)
                        direct_key = f"{instance_name}.{channel_name}"
                        if direct_key in self.telemetry_channels:
                            channel = self.telemetry_channels[direct_key]
                            if channel.size_bytes is not None:
                                total_size += channel.size_bytes
                                resolved = True
                                
                elif len(parts) == 2:
                    # Format: instanceName.ChannelName (no deployment prefix)
                    instance_name = parts[0]
                    channel_name = parts[1]

                    if instance_name in self.instances:
                        component_type = self.instances[instance_name].component_type
                        component_channel_key = f"{component_type}.{channel_name}"

                        if component_channel_key in self.telemetry_channels:
                            channel = self.telemetry_channels[component_channel_key]
                            if channel.size_bytes is not None:
                                total_size += channel.size_bytes
                                resolved = True
                    
                    if not resolved:
                        # Try direct lookup by the path as-is
                        if channel_path in self.telemetry_channels:
                            channel = self.telemetry_channels[channel_path]
                            if channel.size_bytes is not None:
                                total_size += channel.size_bytes
                                resolved = True
                
                # Track unresolved channels
                if not resolved:
                    packet.unresolved_channels.append(channel_path)

            packet.total_size_bytes = total_size

    def analyze(self):
        """Run the full analysis"""
        print("Analyzing F Prime telemetry data budget...")

        # Find and parse all FPP files
        fpp_files = self.find_fpp_files()
        print(f"Found {len(fpp_files)} FPP files")

        # Add common F Prime types that might not be explicitly defined
        self.add_fprime_common_types()

        # Parse type definitions and telemetry channels
        for fpp_file in fpp_files:
            if fpp_file.suffix == ".fppi":
                self.parse_packet_file(fpp_file)
            else:
                self.parse_component_file(fpp_file)

        # Recalculate channel sizes now that all types are loaded
        self.recalculate_channel_sizes()

        # Calculate packet sizes
        self.calculate_packet_sizes()

        print(f"Found {len(self.types)} type definitions")
        print(f"Found {len(self.telemetry_channels)} telemetry channels")
        print(f"Found {len(self.telemetry_packets)} telemetry packets")

    def add_fprime_common_types(self):
        """Add common F Prime framework types"""
        # Fw.TimeValue is typically a struct with seconds and microseconds
        self.types["Fw.TimeValue"] = TypeDefinition(
            name="TimeValue",
            kind="struct",
            size_bytes=12,  # U32 seconds + U32 microseconds + U16 timeBase = 10, rounded to 12
            module="Fw",
        )
        self.types["TimeValue"] = self.types["Fw.TimeValue"]

        # Fw.Time is an alias for TimeValue
        self.types["Fw.Time"] = self.types["Fw.TimeValue"]
        self.types["Time"] = self.types["Fw.TimeValue"]

        # Fw.TimeIntervalValue - similar to TimeValue
        self.types["Fw.TimeIntervalValue"] = TypeDefinition(
            name="TimeIntervalValue",
            kind="struct",
            size_bytes=12,  # U32 seconds + U32 microseconds + U16 timeBase
            module="Fw",
        )
        self.types["TimeIntervalValue"] = self.types["Fw.TimeIntervalValue"]

        # FwSizeType - typically U64 for file sizes
        self.types["FwSizeType"] = TypeDefinition(
            name="FwSizeType", kind="primitive", size_bytes=8
        )

        # Fw.On enum (ON/OFF)
        self.types["Fw.On"] = TypeDefinition(
            name="On", kind="enum", size_bytes=4, module="Fw"
        )
        self.types["On"] = self.types["Fw.On"]

        # String types (need buffer size, defaulting to 80 chars + 2 bytes for size)
        self.types["string"] = TypeDefinition(
            name="string",
            kind="primitive",
            size_bytes=82,  # 80 chars + U16 size field
        )

        # Fw.Success enum
        self.types["Fw.Success"] = TypeDefinition(
            name="Success", kind="enum", size_bytes=4, module="Fw"
        )
        self.types["Success"] = self.types["Fw.Success"]

    def generate_report(self, verbose: bool = False):
        """Generate a comprehensive data budget report"""
        lines = []

        if verbose:
            lines.append("=" * 80)
            lines.append("F PRIME TELEMETRY DATA BUDGET REPORT")
            lines.append("=" * 80)
            lines.append("")

        # Summary statistics
        total_channels = len(self.telemetry_channels)
        channels_with_size = sum(
            1 for ch in self.telemetry_channels.values() if ch.size_bytes is not None
        )
        total_tlm_bytes = sum(
            ch.size_bytes
            for ch in self.telemetry_channels.values()
            if ch.size_bytes is not None
        )

        lines.append("SUMMARY")
        lines.append("-" * 80)
        lines.append(f"Total Telemetry Channels: {total_channels}")
        lines.append(f"Channels with Known Size: {channels_with_size}")
        lines.append(f"Total Telemetry Data: {total_tlm_bytes} bytes")
        lines.append(f"Total Telemetry Packets: {len(self.telemetry_packets)}")
        
        # Show warning if there are unresolved channels
        total_unresolved = sum(len(p.unresolved_channels) for p in self.telemetry_packets.values())
        if total_unresolved > 0:
            lines.append(f"WARNING: {total_unresolved} unresolved channel references")
            if not verbose:
                lines.append("  (Use VERBOSE=1 to see details)")
        
        lines.append("")

        if verbose:
            # Telemetry channels by component
            lines.append("TELEMETRY CHANNELS BY COMPONENT")
            lines.append("-" * 80)
            lines.append(
                f"{'Component':<30} {'Channel':<30} {'Type':<20} {'Size (bytes)':<12}"
            )
            lines.append("-" * 80)

            # Group channels by component
            channels_by_component = {}
            for channel_path, channel in sorted(self.telemetry_channels.items()):
                if channel.component not in channels_by_component:
                    channels_by_component[channel.component] = []
                channels_by_component[channel.component].append((channel_path, channel))

            for component in sorted(channels_by_component.keys()):
                for channel_path, channel in sorted(channels_by_component[component]):
                    size_str = (
                        str(channel.size_bytes)
                        if channel.size_bytes is not None
                        else "UNKNOWN"
                    )
                    lines.append(
                        f"{channel.component:<30} {channel.name:<30} {channel.type_name:<20} {size_str:<12}"
                    )

            lines.append("")

            # Telemetry packets
            lines.append("TELEMETRY PACKETS")
            lines.append("-" * 80)
            lines.append(
                f"{'Packet Name':<25} {'ID':<6} {'Group':<8} {'Channels':<10} {'Total Size (bytes)':<18}"
            )
            lines.append("-" * 80)

            total_packet_bytes = 0
            for packet_name in sorted(self.telemetry_packets.keys()):
                packet = self.telemetry_packets[packet_name]
                lines.append(
                    f"{packet.name:<25} {packet.packet_id:<6} {packet.group_id:<8} {len(packet.channels):<10} {packet.total_size_bytes:<18}"
                )
                total_packet_bytes += packet.total_size_bytes

            lines.append("-" * 80)
            lines.append(
                f"{'TOTAL':<25} {'':<6} {'':<8} {'':<10} {total_packet_bytes:<18}"
            )
            lines.append("")

            # Detailed packet breakdown
            lines.append("DETAILED PACKET BREAKDOWN")
            lines.append("-" * 80)

            for packet_name in sorted(self.telemetry_packets.keys()):
                packet = self.telemetry_packets[packet_name]
                lines.append(
                    f"\nPacket: {packet.name} (ID: {packet.packet_id}, Group: {packet.group_id})"
                )
                lines.append(f"  Total Size: {packet.total_size_bytes} bytes")
                lines.append(f"  Channels ({len(packet.channels)}):")

                for channel_path in packet.channels:
                    # Parse the channel path to find the actual channel
                    parts = channel_path.split(".")
                    channel = None
                    type_name = "UNKNOWN"
                    size_str = "UNKNOWN"

                    if len(parts) >= 3:
                        instance_name = parts[1]
                        channel_name = parts[2]

                        if instance_name in self.instances:
                            component_type = self.instances[
                                instance_name
                            ].component_type
                            component_channel_key = f"{component_type}.{channel_name}"

                            if component_channel_key in self.telemetry_channels:
                                channel = self.telemetry_channels[component_channel_key]
                                type_name = channel.type_name
                                size_str = (
                                    f"{channel.size_bytes} bytes"
                                    if channel.size_bytes is not None
                                    else "UNKNOWN"
                                )
                    elif len(parts) == 2:
                        instance_name = parts[0]
                        channel_name = parts[1]

                        if instance_name in self.instances:
                            component_type = self.instances[
                                instance_name
                            ].component_type
                            component_channel_key = f"{component_type}.{channel_name}"

                            if component_channel_key in self.telemetry_channels:
                                channel = self.telemetry_channels[component_channel_key]
                                type_name = channel.type_name
                                size_str = (
                                    f"{channel.size_bytes} bytes"
                                    if channel.size_bytes is not None
                                    else "UNKNOWN"
                                )

                    lines.append(f"    - {channel_path:<50} {type_name:<20} {size_str}")
                
                # Show unresolved channels if any
                if packet.unresolved_channels:
                    lines.append(f"  Unresolved Channels ({len(packet.unresolved_channels)}):")
                    for unresolved in packet.unresolved_channels:
                        lines.append(f"    ! {unresolved}")

            lines.append("")

        # Bytes per telemetry group (always show)
        lines.append("BYTES PER TELEMETRY GROUP")
        lines.append("-" * 80)

        # Group descriptions (from ReferenceDeploymentPackets.fppi)
        group_descriptions = {
            1: "Beacon",
            2: "Live Satellite Sensor Data",
            3: "Satellite Meta Data",
            4: "Payload Meta Data",
            5: "Health and Status",
            6: "Parameters",
        }

        # Calculate bytes per group
        bytes_per_group = {}
        for packet in self.telemetry_packets.values():
            group_id = packet.group_id
            if group_id not in bytes_per_group:
                bytes_per_group[group_id] = 0
            bytes_per_group[group_id] += packet.total_size_bytes

        # Display group summary
        lines.append(
            f"{'Group':<8} {'Description':<35} {'Packets':<10} {'Total Size (bytes)':<18}"
        )
        lines.append("-" * 80)

        total_group_bytes = 0
        for group_id in sorted(bytes_per_group.keys()):
            group_desc = group_descriptions.get(group_id, f"Group {group_id}")
            packet_count = sum(
                1 for p in self.telemetry_packets.values() if p.group_id == group_id
            )
            group_bytes = bytes_per_group[group_id]
            total_group_bytes += group_bytes
            lines.append(
                f"{group_id:<8} {group_desc:<35} {packet_count:<10} {group_bytes:<18}"
            )

        lines.append("-" * 80)
        lines.append(f"{'TOTAL':<8} {'':<35} {'':<10} {total_group_bytes:<18}")

        if verbose:
            lines.append("")
            lines.append("=" * 80)

        # Output to console
        report_text = "\n".join(lines)
        print(report_text)


def main():
    """Main entry point"""
    parser = argparse.ArgumentParser(
        description="Analyze F Prime telemetry data budget",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=__doc__,
    )
    parser.add_argument(
        "--verbose",
        "-v",
        action="store_true",
        help="Show detailed breakdown (channels, packets, and detailed packet info)",
        default=False,
    )
    parser.add_argument(
        "--project-root",
        "-p",
        help="Project root directory (default: current directory)",
        default=".",
    )

    args = parser.parse_args()

    project_root = Path(args.project_root).resolve()
    if not project_root.exists():
        print(f"Error: Project root does not exist: {project_root}", file=sys.stderr)
        return 1

    analyzer = DataBudgetAnalyzer(project_root)
    analyzer.analyze()
    analyzer.generate_report(verbose=args.verbose)

    return 0


if __name__ == "__main__":
    sys.exit(main())

#!/usr/bin/env python3
import argparse
import zlib
from pathlib import Path

# See if PyYAML is available, if not set yaml to None
try:
    import yaml
except ImportError:
    yaml = None


def parse_args():
    """Parse command line arguments

    Parse command line arguments for the file-analyse tool. These include:
    - file: The first file to analyze (positional argument)
    - -c/--config: Optional configuration file (YAML format), defaults to ./fprime-gds.yml
    - -t/--truth: Truth file for comparison (optional)
    - --chunk-size: Size of chunks to read from the file (default: read from config)
    - -h/--help: Show help message and exit
    """
    parser = argparse.ArgumentParser(description="Analyze file chunk-by-chunk")
    parser.add_argument("file", type=Path, help="The file to analyse")
    parser.add_argument(
        "-c",
        "--config",
        type=Path,
        default=Path("./fprime-gds.yml"),
        help="Optional configuration file in YAML format (default: ./fprime-gds.yml).",
    )
    parser.add_argument(
        "-t",
        "--truth",
        type=Path,
        default=None,
        help="Truth file for comparison (optional).",
    )
    parser.add_argument(
        "--chunk-size",
        type=int,
        default=None,
        help="Size of chunks to read from the file (default: read from config).",
    )
    parsed_args = parser.parse_args()

    # Read config file if it exists and PyYAML is available
    try:
        with open(parsed_args.config, "r") as file_handle:
            config_data = yaml.safe_load(file_handle)
            parsed_args.chunk_size = (
                parsed_args.chunk_size
                if parsed_args.chunk_size is not None
                else config_data.get("command-line-options", {}).get(
                    "file-uplink-chunk-size", None
                )
            )
    except Exception:
        pass

    # Validate that chunk_size is set
    if parsed_args.chunk_size is None and yaml is None:
        parser.error("PyYAML is required or --chunk-size must be specified.")
    elif parsed_args.chunk_size is None and not Path(parsed_args.config).is_file():
        parser.error("Configuration file required or --chunk-size must be specified.")
    elif parsed_args.chunk_size is None:
        parser.error("Chunk size must be specified in config or via --chunk-size.")

    # Validate that the file exists
    if not parsed_args.file.is_file():
        parser.error(f"The file {parsed_args.file} does not exist.")
    # Validate that the truth file exists if provided
    if parsed_args.truth and not parsed_args.truth.is_file():
        parser.error(f"The truth file {parsed_args.truth} does not exist.")
    return parsed_args


def count_errors(data: bytes, truth: bytes) -> int:
    """Count the number of byte errors between data and truth

    Compares two byte sequences and counts the number of differing bytes.

    Args:
        data (bytes): The data bytes.
        truth (bytes): The truth bytes.

    Returns:
        int: The number of differing bytes.
    """
    return sum(
        1 for d_byte, t_byte in zip(data, truth) if d_byte != t_byte and d_byte != 0
    ), sum(1 for d_byte, t_byte in zip(data, truth) if d_byte != t_byte and d_byte == 0)


def process(file: Path, chunk: int, truth: Path = None):
    """Process the file chunk-by-chunk

    Processes the given file chunk-by-chunk. If a truth file is provided, compares each chunk against the corresponding
    chunk in the truth file and will report any discrepancies. If no truth file is provided, then chunks composed of
    all zero bytes are reported (likely indicating a dropped packet).

    Args:
        file (Path): The file to process.
        chunk (int): The size of each chunk to read.
        truth (Path, optional): The truth file for comparison. Defaults to None.
    Returns:
        ???
    """
    return_data = {"possible_drops": [], "known_packets": [], "mismatched_packets": []}
    data_crc = 0
    truth_crc = 0

    with open(file, "rb") as file_handle:
        with open(truth if truth is not None else "/dev/null", "rb") as truth_handle:
            packet_count = 0
            data = None
            truth_data = None
            while data != b"" or (truth is not None and truth_data != b""):
                position = file_handle.tell()
                data = file_handle.read(chunk)
                truth_data = truth_handle.read(chunk) if truth_handle else None

                # Compare with chuck * 0 if no truth file is provided
                if truth is None and data == b"\x00" * chunk:
                    return_data["possible_drops"].append((packet_count, position))
                # Truth file provided, compare against it. If data is all zeros, report as dropped packet
                elif (
                    truth is not None
                    and data != truth_data
                    and (data == b"\x00" * chunk or data == b"")
                ):
                    return_data["known_packets"].append((packet_count, position))
                # Truth file provided, compare against it. If data mismatches truth, report as mismatched packet
                elif truth is not None and data != truth_data:
                    return_data["mismatched_packets"].append(
                        (packet_count, position, count_errors(data, truth_data))
                    )
                truth_crc = (
                    zlib.crc32(truth_data, truth_crc) if truth is not None else 0
                )
                data_crc = zlib.crc32(data, data_crc)
                packet_count += 1
    return_data["data_crc"] = ~data_crc & 0xFFFFFFFF
    return_data["truth_crc"] = ~truth_crc & 0xFFFFFFFF
    return return_data


def main():
    """Process the file in chunks based on command line arguments"""
    args = parse_args()
    results = process(args.file, args.chunk_size, args.truth)
    print("File Analysis Results:")
    print(f"Data CRC32: {results['data_crc']:08X}")
    if args.truth:
        print(f"Truth CRC32: {results['truth_crc']:08X}")
    print()
    print(f"Possible Drops: {len(results['possible_drops'])}")
    for packet_num, position in results["possible_drops"]:
        print(f"  Packet {packet_num} at position {position}")
    print()
    print(f"Known Drops: {len(results['known_packets'])}")
    for packet_num, position in results["known_packets"]:
        print(f"  Packet {packet_num} at position {position}")
    print()
    print(f"Mismatched Packets: {len(results['mismatched_packets'])}")
    for packet_num, position, (non_zero_error_count, zero_error_count) in results[
        "mismatched_packets"
    ]:
        print(
            f"  Packet {packet_num} at position {position} with {non_zero_error_count} non-zero errors and {zero_error_count} zero errors"
        )


if __name__ == "__main__":
    main()

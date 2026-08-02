#!/usr/bin/env python3
"""Decode a TlmArchive CSV file with an F Prime topology dictionary."""

from __future__ import annotations

import argparse
import csv
import json
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import Any, Iterable, TextIO

ARCHIVE_FIELDS = ("format_version", "packet_size_bytes", "packet_hex")
ARCHIVE_FORMAT_VERSION = 1
PACKETIZED_TLM_DESCRIPTOR = 0x0004


class ArchiveDecodeError(ValueError):
    """Report malformed archive records and undecodable telemetry packets."""


@dataclass(frozen=True)
class ArchiveRecord:
    """One packet record read from the telemetry archive."""

    record_number: int
    payload: bytes


@dataclass(frozen=True)
class DecodedPacket:
    """Human-readable metadata and channel values for one packet."""

    record_number: int
    packet_id: int
    packet_name: str
    packet_size_bytes: int
    time_base: str
    time_context: int
    seconds: int
    microseconds: int
    channels: tuple[dict[str, Any], ...]


def read_archive(stream: TextIO) -> Iterable[ArchiveRecord]:
    """Yield validated packets from a TlmArchive CSV stream."""

    reader = csv.DictReader(stream)
    if reader.fieldnames != list(ARCHIVE_FIELDS):
        actual = ",".join(reader.fieldnames or ()) or "<missing>"
        expected = ",".join(ARCHIVE_FIELDS)
        raise ArchiveDecodeError(f"archive header is {actual!r}; expected {expected!r}")

    for record_number, row in enumerate(reader, start=1):
        line_number = reader.line_num
        try:
            version = int(row["format_version"])
            declared_size = int(row["packet_size_bytes"])
        except (TypeError, ValueError) as exc:
            raise ArchiveDecodeError(
                f"line {line_number}: version and packet size must be integers"
            ) from exc

        if version != ARCHIVE_FORMAT_VERSION:
            raise ArchiveDecodeError(
                f"line {line_number}: unsupported archive format version {version}"
            )
        if declared_size < 0:
            raise ArchiveDecodeError(
                f"line {line_number}: packet size cannot be negative"
            )

        packet_hex = row["packet_hex"]
        try:
            payload = bytes.fromhex(packet_hex)
        except (TypeError, ValueError) as exc:
            raise ArchiveDecodeError(
                f"line {line_number}: packet_hex is not valid hexadecimal"
            ) from exc
        if len(payload) != declared_size:
            raise ArchiveDecodeError(
                f"line {line_number}: declared {declared_size} packet bytes, "
                f"decoded {len(payload)}"
            )

        yield ArchiveRecord(record_number, payload)


class TelemetryDecoder:
    """Decode packetized telemetry using the project's generated dictionary."""

    def __init__(self, dictionary_path: Path, packet_set_name: str | None = None):
        """Load the dictionary and configure the standard F Prime decoder."""

        try:
            from fprime_gds.common.decoders.pkt_decoder import PktDecoder
            from fprime_gds.common.models.dictionaries import Dictionaries
            from fprime_gds.common.models.serialize.time_type import TimeType
            from fprime_gds.common.utils.config_manager import ConfigManager
        except ImportError as exc:
            raise ArchiveDecodeError(
                "fprime-gds is unavailable; run 'make fprime-venv' and invoke this "
                "script with 'fprime-venv/bin/python'"
            ) from exc

        if not dictionary_path.is_file():
            raise ArchiveDecodeError(f"dictionary does not exist: {dictionary_path}")

        try:
            self._dictionaries = Dictionaries.load_dictionaries_into_config(
                str(dictionary_path), packet_set_name=packet_set_name
            )
        except Exception as exc:
            raise ArchiveDecodeError(f"could not load dictionary: {exc}") from exc

        if not self._dictionaries.packet:
            raise ArchiveDecodeError("dictionary contains no telemetry packet set")

        self._config = ConfigManager()
        self._time_type = TimeType
        self._decoder = PktDecoder(
            self._dictionaries.packet, self._dictionaries.channel_id
        )

    def decode(self, record: ArchiveRecord) -> DecodedPacket:
        """Decode one validated archive record."""

        payload = record.payload
        descriptor_type = self._config.get_type("FwPacketDescriptorType")()
        packet_id_type = self._config.get_type("FwTlmPacketizeIdType")()

        minimum_size = (
            descriptor_type.getSize()
            + packet_id_type.getSize()
            + self._time_type.getSize()
        )
        if len(payload) < minimum_size:
            raise ArchiveDecodeError(
                f"record {record.record_number}: packet is {len(payload)} bytes; "
                f"the packetized telemetry header requires {minimum_size}"
            )

        try:
            descriptor_type.deserialize(payload, 0)
            descriptor = descriptor_type.val
            descriptor_size = descriptor_type.getSize()
            packet_id_type.deserialize(payload, descriptor_size)
            packet_id = packet_id_type.val
        except Exception as exc:
            raise ArchiveDecodeError(
                f"record {record.record_number}: invalid packet header: {exc}"
            ) from exc

        if descriptor != PACKETIZED_TLM_DESCRIPTOR:
            raise ArchiveDecodeError(
                f"record {record.record_number}: descriptor 0x{descriptor:04X} is not "
                "packetized telemetry (0x0004)"
            )
        if packet_id not in self._dictionaries.packet:
            raise ArchiveDecodeError(
                f"record {record.record_number}: packet ID {packet_id} is not in the dictionary"
            )

        packet_template = self._dictionaries.packet[packet_id]
        expected_size = minimum_size + sum(
            channel.get_type_obj().getMaxSize()
            for channel in packet_template.get_ch_list()
        )
        if len(payload) != expected_size:
            raise ArchiveDecodeError(
                f"record {record.record_number}: packet ID {packet_id} is {len(payload)} "
                f"bytes; dictionary expects {expected_size}"
            )

        packet_time = self._time_type()
        try:
            packet_time.deserialize(payload, descriptor_size + packet_id_type.getSize())
            channel_data = self._decoder.decode_api(payload[descriptor_size:])
        except Exception as exc:
            raise ArchiveDecodeError(
                f"record {record.record_number}: packet ID {packet_id} failed to decode: {exc}"
            ) from exc

        channels = tuple(
            {
                "name": channel.template.get_full_name(),
                "id": channel.id,
                "value": channel.get_val(),
                "display": str(channel.get_display_text()),
            }
            for channel in channel_data
        )
        return DecodedPacket(
            record_number=record.record_number,
            packet_id=packet_id,
            packet_name=packet_template.get_name(),
            packet_size_bytes=len(payload),
            time_base=str(packet_time.timeBase.val),
            time_context=packet_time.timeContext,
            seconds=packet_time.seconds,
            microseconds=packet_time.useconds,
            channels=channels,
        )


def packet_to_dict(packet: DecodedPacket) -> dict[str, Any]:
    """Convert a decoded packet to JSON-compatible primitives."""

    return {
        "record_number": packet.record_number,
        "packet_id": packet.packet_id,
        "packet_name": packet.packet_name,
        "packet_size_bytes": packet.packet_size_bytes,
        "timestamp": {
            "time_base": packet.time_base,
            "context": packet.time_context,
            "seconds": packet.seconds,
            "microseconds": packet.microseconds,
        },
        "channels": list(packet.channels),
    }


def write_text(packets: Iterable[DecodedPacket], stream: TextIO) -> None:
    """Write decoded packets in a compact human-readable form."""

    for packet_index, packet in enumerate(packets):
        if packet_index:
            stream.write("\n")
        stream.write(
            f"Packet {packet.record_number}: {packet.packet_name} "
            f"(ID {packet.packet_id}, {packet.packet_size_bytes} bytes)\n"
        )
        stream.write(
            f"  Time: {packet.time_base}, context {packet.time_context}, "
            f"{packet.seconds}.{packet.microseconds:06d}\n"
        )
        for channel in packet.channels:
            stream.write(f"  {channel['name']} = {channel['display']}\n")


def write_csv(packets: Iterable[DecodedPacket], stream: TextIO) -> None:
    """Write one long-form CSV row per decoded telemetry channel."""

    writer = csv.writer(stream, lineterminator="\n")
    writer.writerow(
        (
            "record_number",
            "packet_name",
            "packet_id",
            "time_base",
            "time_context",
            "seconds",
            "microseconds",
            "channel_name",
            "channel_id",
            "value",
        )
    )
    for packet in packets:
        for channel in packet.channels:
            writer.writerow(
                (
                    packet.record_number,
                    packet.packet_name,
                    packet.packet_id,
                    packet.time_base,
                    packet.time_context,
                    packet.seconds,
                    packet.microseconds,
                    channel["name"],
                    channel["id"],
                    json.dumps(channel["value"], separators=(",", ":")),
                )
            )


def parse_args(argv: list[str] | None = None) -> argparse.Namespace:
    """Parse command-line arguments."""

    parser = argparse.ArgumentParser(
        description="Decode pre_deployment.csv into named telemetry values."
    )
    parser.add_argument("archive", type=Path, help="downloaded TlmArchive CSV file")
    parser.add_argument(
        "dictionary", type=Path, help="matching F Prime topology dictionary JSON"
    )
    parser.add_argument(
        "--packet-set",
        help="telemetry packet set name (only needed when the dictionary has several)",
    )
    parser.add_argument(
        "--format",
        choices=("text", "csv", "json"),
        default="text",
        help="output format (default: text)",
    )
    parser.add_argument("--output", type=Path, help="write output to this file")
    return parser.parse_args(argv)


def main(argv: list[str] | None = None) -> int:
    """Run the archive decoder command."""

    args = parse_args(argv)
    try:
        decoder = TelemetryDecoder(args.dictionary, args.packet_set)
        with args.archive.open("r", encoding="ascii", newline="") as archive_stream:
            packets = [
                decoder.decode(record) for record in read_archive(archive_stream)
            ]

        output_stream = (
            args.output.open("w", encoding="utf-8", newline="")
            if args.output
            else sys.stdout
        )
        try:
            if args.format == "text":
                write_text(packets, output_stream)
            elif args.format == "csv":
                write_csv(packets, output_stream)
            else:
                json.dump(
                    [packet_to_dict(packet) for packet in packets],
                    output_stream,
                    indent=2,
                )
                output_stream.write("\n")
        finally:
            if args.output:
                output_stream.close()
    except (ArchiveDecodeError, OSError) as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

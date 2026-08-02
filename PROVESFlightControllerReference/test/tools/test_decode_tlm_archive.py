"""Tests for the ground-side telemetry archive decoder."""

from __future__ import annotations

import io
from pathlib import Path

import pytest

from tools.decode_tlm_archive import (
    ArchiveDecodeError,
    TelemetryDecoder,
    read_archive,
)

PROJECT_ROOT = Path(__file__).parents[3]
DICTIONARY = (
    PROJECT_ROOT
    / "build-artifacts/zephyr/fprime-zephyr-deployment/dict"
    / "ReferenceDeploymentTopologyDictionary.json"
)


def make_filesystem_packet() -> bytes:
    """Create a real packet ID 5 using types loaded from the flight dictionary."""

    from fprime_gds.common.models.dictionaries import Dictionaries
    from fprime_gds.common.models.serialize.time_type import TimeType
    from fprime_gds.common.utils.config_manager import ConfigManager

    dictionaries = Dictionaries.load_dictionaries_into_config(str(DICTIONARY))
    config = ConfigManager()
    packet = dictionaries.packet[5]
    return b"".join(
        (
            config.get_type("FwPacketDescriptorType")(4).serialize(),
            config.get_type("FwTlmPacketizeIdType")(5).serialize(),
            TimeType(TimeType.TimeBase("TB_PROC_TIME"), 2, 123, 456).serialize(),
            packet.get_ch_list()[0].get_type_obj()(4096).serialize(),
            packet.get_ch_list()[1].get_type_obj()(8192).serialize(),
        )
    )


@pytest.mark.skipif(not DICTIONARY.is_file(), reason="generated dictionary unavailable")
def test_decode_packet_uses_dictionary_names_and_types() -> None:
    """Decode packet boundaries, metadata, and typed channel values."""

    payload = make_filesystem_packet()
    archive = io.StringIO(
        "format_version,packet_size_bytes,packet_hex\n"
        f"1,{len(payload)},{payload.hex().upper()}\n"
    )
    record = next(iter(read_archive(archive)))
    decoded = TelemetryDecoder(DICTIONARY).decode(record)

    assert decoded.packet_name == "FileSystem"
    assert decoded.packet_id == 5
    assert decoded.time_base == "TB_PROC_TIME"
    assert decoded.time_context == 2
    assert decoded.seconds == 123
    assert decoded.microseconds == 456
    assert [channel["name"] for channel in decoded.channels] == [
        "ReferenceDeployment.fsSpace.FreeSpace",
        "ReferenceDeployment.fsSpace.TotalSpace",
    ]
    assert [channel["value"] for channel in decoded.channels] == [4096, 8192]


def test_archive_rejects_size_mismatch() -> None:
    """Reject truncated rows before asking F Prime to deserialize them."""

    archive = io.StringIO("format_version,packet_size_bytes,packet_hex\n1,3,0001\n")
    with pytest.raises(ArchiveDecodeError, match="declared 3 packet bytes, decoded 2"):
        list(read_archive(archive))

"""Tests for the PROVES YAMCS adapter routing and config loading."""

from pathlib import Path

import pytest

from tools.yamcs.proves_adapter import (
    YamcsDeployment,
    _crc16_ccitt,
    _extract_tm_scid,
    _tm_route_for_frame,
    load_yamcs_deployments,
)


def _write_deployments(tmp_path: Path, body: str) -> Path:
    """Write a deployments.yaml with the given body."""
    path = tmp_path / "deployments.yaml"
    path.write_text(body)
    return path


def _tm_frame(scid: int, vc_id: int = 1, frame_length: int = 32) -> bytes:
    """Build a CRC-valid TM frame for the given SCID."""
    frame = bytearray(frame_length)
    word0 = (scid << 4) | (vc_id << 1)
    frame[0] = (word0 >> 8) & 0xFF
    frame[1] = word0 & 0xFF
    frame[3] = 7
    crc = _crc16_ccitt(bytes(frame[:-2]))
    frame[-2] = (crc >> 8) & 0xFF
    frame[-1] = crc & 0xFF
    return bytes(frame)


def test_load_yamcs_deployments_accepts_valid_config(tmp_path):
    """A valid two-deployment config loads with all fields."""
    path = _write_deployments(
        tmp_path,
        """
deployments:
  - name: proves-68
    spacecraft_id: 68
    vc_id: 1
    yamcs_host: 127.0.0.1
    tm_port: 50000
    tc_port: 50001
  - name: proves-67
    spacecraft_id: 67
    vc_id: 1
    yamcs_host: 127.0.0.1
    tm_port: 50100
    tc_port: 50101
""",
    )

    deployments = load_yamcs_deployments(path)

    assert deployments == [
        YamcsDeployment("proves-68", 68, 1, "127.0.0.1", 50000, 50001),
        YamcsDeployment("proves-67", 67, 1, "127.0.0.1", 50100, 50101),
    ]


@pytest.mark.parametrize(
    ("field", "replacement", "message"),
    [
        ("spacecraft_id: 67", "spacecraft_id: 68", "duplicate spacecraft_id"),
        ("tc_port: 50101", "tc_port: 50001", "duplicate tc_port"),
        ("spacecraft_id: 67", "spacecraft_id: 1024", "out of range"),
        ("vc_id: 1", "vc_id: 8", "out of range"),
    ],
)
def test_load_yamcs_deployments_rejects_invalid_config(
    tmp_path, field, replacement, message
):
    """Invalid configs raise ValueError."""
    config = """
deployments:
  - name: proves-68
    spacecraft_id: 68
    vc_id: 1
    yamcs_host: 127.0.0.1
    tm_port: 50000
    tc_port: 50001
  - name: proves-67
    spacecraft_id: 67
    vc_id: 1
    yamcs_host: 127.0.0.1
    tm_port: 50100
    tc_port: 50101
"""
    path = _write_deployments(tmp_path, config.replace(field, replacement, 1))

    with pytest.raises(ValueError, match=message):
        load_yamcs_deployments(path)


def test_tm_route_for_frame_maps_configured_scid_without_rewriting():
    """Configured SCIDs route to their deployment unmodified."""
    deployment = YamcsDeployment("proves-68", 68, 1, "127.0.0.1", 50000, 50001)
    frame = _tm_frame(68)

    route = _tm_route_for_frame(frame, {deployment.spacecraft_id: deployment})

    assert route == deployment
    assert _extract_tm_scid(frame) == 68


def test_tm_route_for_frame_drops_unknown_scid():
    """Unconfigured SCIDs route to None."""
    deployment = YamcsDeployment("proves-68", 68, 1, "127.0.0.1", 50000, 50001)
    frame = _tm_frame(67)

    route = _tm_route_for_frame(frame, {deployment.spacecraft_id: deployment})

    assert route is None
    assert _extract_tm_scid(frame) == 67


def test_log_unknown_frame_appends_json_lines(tmp_path):
    """Unknown-SCID frames append one parseable JSON record per line."""
    import json

    from tools.yamcs.proves_adapter import _log_unknown_frame

    frame = _tm_frame(99)
    log_path = tmp_path / "unknown_pkts.json"

    _log_unknown_frame(log_path, frame)
    _log_unknown_frame(log_path, frame)

    lines = log_path.read_text().splitlines()
    assert len(lines) == 2
    record = json.loads(lines[0])
    assert record["scid"] == 99
    assert record["vc_id"] == 1
    assert record["vc_count"] == 7
    assert record["length"] == len(frame)
    assert record["frame_hex"] == frame.hex()
    assert "time" in record

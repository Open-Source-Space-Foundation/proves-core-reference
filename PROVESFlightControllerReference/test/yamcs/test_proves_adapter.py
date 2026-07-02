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
    path = tmp_path / "deployments.yaml"
    path.write_text(body)
    return path


def _tm_frame(scid: int, vc_id: int = 1, frame_length: int = 32) -> bytes:
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
    deployment = YamcsDeployment("proves-68", 68, 1, "127.0.0.1", 50000, 50001)
    frame = _tm_frame(68)

    route = _tm_route_for_frame(frame, {deployment.spacecraft_id: deployment})

    assert route == deployment
    assert _extract_tm_scid(frame) == 68


def test_tm_route_for_frame_drops_unknown_scid():
    deployment = YamcsDeployment("proves-68", 68, 1, "127.0.0.1", 50000, 50001)
    frame = _tm_frame(67)

    route = _tm_route_for_frame(frame, {deployment.spacecraft_id: deployment})

    assert route is None
    assert _extract_tm_scid(frame) == 67

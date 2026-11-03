import json
from pathlib import Path

import pytest
import yaml

from tools.yamcs.generate_deployments import (
    _default_name,
    generate,
)
from tools.yamcs.proves_adapter import load_yamcs_deployments


def _write_dictionary(
    tmp_path: Path,
    filename: str,
    deployment_name: str = "ReferenceDeployment.ReferenceDeployment",
    spacecraft_id: int = 17,
    frame_size: int = 248,
) -> Path:
    """Write a minimal F Prime dictionary JSON with ComCfg constants."""
    path = tmp_path / filename
    path.write_text(
        json.dumps(
            {
                "metadata": {"deploymentName": deployment_name},
                "constants": [
                    {"qualifiedName": "ComCfg.SpacecraftId", "value": spacecraft_id},
                    {"qualifiedName": "ComCfg.TmFrameFixedSize", "value": frame_size},
                ],
            }
        )
    )
    return path


@pytest.fixture
def fake_xtce(monkeypatch):
    """Replace fprime-to-xtce with a stub that writes a minimal XTCE file."""

    def _fake(dictionary: Path, output: Path) -> None:
        deployment_name = json.load(dictionary.open())["metadata"]["deploymentName"]
        root = deployment_name.replace(".", "_")
        output.write_text(
            f'<?xml version="1.0" encoding="utf-8"?>\n'
            f'<SpaceSystem xmlns="http://www.omg.org/spec/XTCE/20180204" '
            f'name="{root}"/>\n'
        )

    monkeypatch.setattr("tools.yamcs.generate_deployments._run_fprime_to_xtce", _fake)


def test_generate_two_deployments(tmp_path, fake_xtce):
    """Two dictionaries produce two instances with allocated ports."""
    dict_a = _write_dictionary(tmp_path, "a.json", spacecraft_id=17)
    dict_b = _write_dictionary(
        tmp_path, "b.json", deployment_name="OtherSat.OtherSat", spacecraft_id=68
    )
    output = tmp_path / "deployments"

    specs = generate([dict_a, dict_b], output=output)

    assert [(s.name, s.spacecraft_id, s.tm_port, s.tc_port) for s in specs] == [
        ("ReferenceDeployment-scid17", 17, 50000, 50001),
        ("OtherSat-scid68", 68, 50002, 50003),
    ]

    # Server config lists both instances
    server = yaml.safe_load((output / "etc" / "yamcs.yaml").read_text())
    assert server["instances"] == ["ReferenceDeployment-scid17", "OtherSat-scid68"]
    assert (output / "etc" / "processor.yaml").is_file()

    # Per-instance config carries the rewritten links, mdb path, rootContainer
    instance = yaml.safe_load(
        (output / "etc" / "yamcs.OtherSat-scid68.yaml").read_text()
    )
    tm_link = next(
        link
        for link in instance["dataLinks"]
        if link["class"] == "org.yamcs.tctm.ccsds.UdpTmFrameLink"
    )
    tc_link = next(
        link
        for link in instance["dataLinks"]
        if link["class"] == "org.yamcs.tctm.ccsds.UdpTcFrameLink"
    )
    assert tm_link["port"] == 50002
    assert tm_link["spacecraftId"] == 68
    assert tm_link["frameLength"] == 248
    assert tm_link["virtualChannels"][0]["maxPacketLength"] == 248
    assert tc_link["port"] == 50003
    assert tc_link["spacecraftId"] == 68
    assert tc_link["maxFrameLength"] == 248
    assert instance["mdb"][0]["args"]["file"] == "mdb/OtherSat-scid68.xtce.xml"
    tm_stream = next(
        s for s in instance["streamConfig"]["tm"] if s["name"] == "tm_realtime"
    )
    assert tm_stream["rootContainer"] == "/OtherSat_OtherSat/CCSDSSpacePacket"
    assert (output / "mdb" / "OtherSat-scid68.xtce.xml").is_file()


def test_generated_deployments_yaml_loads_in_adapter(tmp_path, fake_xtce):
    """The generated routing table loads through the adapter."""
    dict_a = _write_dictionary(tmp_path, "a.json", spacecraft_id=17)
    dict_b = _write_dictionary(
        tmp_path, "b.json", deployment_name="OtherSat.OtherSat", spacecraft_id=68
    )
    output = tmp_path / "deployments"

    generate([dict_a, dict_b], output=output)

    deployments = load_yamcs_deployments(output / "deployments.yaml")
    assert [(d.name, d.spacecraft_id, d.tm_port, d.tc_port) for d in deployments] == [
        ("ReferenceDeployment-scid17", 17, 50000, 50001),
        ("OtherSat-scid68", 68, 50002, 50003),
    ]
    # The extra dictionary key survives for launch tooling
    doc = yaml.safe_load((output / "deployments.yaml").read_text())
    assert doc["deployments"][0]["dictionary"] == str(dict_a.resolve())


def test_duplicate_scids_rejected(tmp_path, fake_xtce):
    """Two dictionaries with the same SCID are rejected."""
    dict_a = _write_dictionary(tmp_path, "a.json", spacecraft_id=17)
    dict_b = _write_dictionary(
        tmp_path, "b.json", deployment_name="OtherSat.OtherSat", spacecraft_id=17
    )

    with pytest.raises(ValueError, match="duplicate spacecraft ID 17"):
        generate([dict_a, dict_b], output=tmp_path / "out")


def test_duplicate_names_rejected(tmp_path, fake_xtce):
    """Explicit duplicate instance names are rejected."""
    dict_a = _write_dictionary(tmp_path, "a.json", spacecraft_id=17)
    dict_b = _write_dictionary(tmp_path, "b.json", spacecraft_id=68)

    with pytest.raises(ValueError, match="duplicate deployment name"):
        generate([dict_a, dict_b], output=tmp_path / "out", names=["same", "same"])


def test_mismatched_frame_sizes_rejected(tmp_path, fake_xtce):
    """Dictionaries disagreeing on TmFrameFixedSize are rejected."""
    dict_a = _write_dictionary(tmp_path, "a.json", spacecraft_id=17, frame_size=248)
    dict_b = _write_dictionary(
        tmp_path,
        "b.json",
        deployment_name="OtherSat.OtherSat",
        spacecraft_id=68,
        frame_size=1024,
    )

    with pytest.raises(ValueError, match="mismatched ComCfg.TmFrameFixedSize"):
        generate([dict_a, dict_b], output=tmp_path / "out")


def test_missing_spacecraft_id_rejected(tmp_path, fake_xtce):
    """A dictionary without ComCfg.SpacecraftId is rejected."""
    path = tmp_path / "a.json"
    path.write_text(
        json.dumps(
            {
                "metadata": {"deploymentName": "Ref.Ref"},
                "constants": [
                    {"qualifiedName": "ComCfg.TmFrameFixedSize", "value": 248}
                ],
            }
        )
    )

    with pytest.raises(ValueError, match="ComCfg.SpacecraftId"):
        generate([path], output=tmp_path / "out")


def test_default_name_sanitized():
    """Default names use the YAMCS instance-name charset."""
    assert _default_name("Ref Deploy.Ref", 7) == "Ref_Deploy-scid7"

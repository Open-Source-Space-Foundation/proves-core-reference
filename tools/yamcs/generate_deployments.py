"""Generate a multi-instance YAMCS deployment tree from F Prime dictionaries.

Takes one or more F Prime topology dictionaries (*TopologyDictionary.json) and
produces a single-server, multi-instance YAMCS configuration directory:

    <output>/
      etc/yamcs.yaml             one HTTP server (:8090), all instances listed
      etc/yamcs.<name>.yaml      per-deployment data links (unique TM/TC ports)
      etc/processor.yaml         copied from the checked-in template
      mdb/<name>.xtce.xml        XTCE generated from each dictionary
      deployments.yaml           routing table consumed by proves_adapter.py

Each dictionary self-describes its deployment: the instance name derives from
metadata.deploymentName and the ComCfg.SpacecraftId constant, and the frame
length comes from ComCfg.TmFrameFixedSize.  TM/TC UDP port pairs are allocated
sequentially from --base-port so no two deployments conflict.

Usage:
    python tools/yamcs/generate_deployments.py DICT.json [DICT2.json ...] \
        [--output yamcs/deployments] [--base-port 50000] [--name NAME ...]
"""

import argparse
import json
import re
import shutil
import subprocess
import sys
import xml.etree.ElementTree as ET
from dataclasses import dataclass
from pathlib import Path

import yaml

REPO_ROOT = Path(__file__).resolve().parents[2]
DEFAULT_TEMPLATE_DIR = REPO_ROOT / "yamcs" / "yamcs-data"
DEFAULT_OUTPUT_DIR = REPO_ROOT / "yamcs" / "deployments"

# Dictionary constants required from each F Prime dictionary
_SCID_CONSTANT = "ComCfg.SpacecraftId"
_FRAME_SIZE_CONSTANT = "ComCfg.TmFrameFixedSize"


@dataclass(frozen=True)
class DeploymentSpec:
    """One YAMCS instance derived from an F Prime dictionary."""

    name: str
    dictionary: Path
    spacecraft_id: int
    frame_length: int
    vc_id: int
    yamcs_host: str
    tm_port: int
    tc_port: int


def _read_dictionary(path: Path) -> tuple[str, int, int]:
    """Return (deploymentName, spacecraft_id, frame_length) from a dictionary."""
    with path.open() as stream:
        data = json.load(stream)

    deployment_name = data.get("metadata", {}).get("deploymentName")
    if not deployment_name:
        raise ValueError(f"{path}: missing metadata.deploymentName")

    constants = {
        constant.get("qualifiedName"): constant.get("value")
        for constant in data.get("constants", [])
    }
    missing = [
        name for name in (_SCID_CONSTANT, _FRAME_SIZE_CONSTANT) if name not in constants
    ]
    if missing:
        raise ValueError(
            f"{path}: missing dictionary constant(s): {', '.join(missing)}"
        )
    return (
        deployment_name,
        int(constants[_SCID_CONSTANT]),
        int(constants[_FRAME_SIZE_CONSTANT]),
    )


def _default_name(deployment_name: str, spacecraft_id: int) -> str:
    """Instance name from the dictionary deployment name and spacecraft ID."""
    base = deployment_name.split(".")[0]
    return _sanitize_name(f"{base}-scid{spacecraft_id}")


def _sanitize_name(name: str) -> str:
    """Restrict to the YAMCS instance-name character set."""
    return re.sub(r"[^A-Za-z0-9_-]", "_", name)


def _run_fprime_to_xtce(dictionary: Path, output: Path) -> None:
    """Convert an F Prime dictionary to XTCE (same invocation as `make yamcs-dict`)."""
    subprocess.run(["fprime-to-xtce", str(dictionary), "-o", str(output)], check=True)


def _xtce_root_container(xtce_path: Path) -> str:
    """Derive the tm_realtime rootContainer from the XTCE root SpaceSystem name."""
    root_name = ET.parse(xtce_path).getroot().get("name")
    if not root_name:
        raise ValueError(f"{xtce_path}: XTCE root SpaceSystem has no name attribute")
    return f"/{root_name}/CCSDSSpacePacket"


def _render_instance_config(
    template: dict, spec: DeploymentSpec, root_container: str
) -> dict:
    """Rewrite the instance template for one deployment.

    Mirrors fprime_yamcs.construct_temporary_configuration: ports, spacecraftId
    and frame lengths on the UDP frame links, the MDB file path, and the
    tm_realtime rootContainer.
    """
    config = yaml.safe_load(yaml.safe_dump(template))  # deep copy

    for link in config.get("dataLinks", []):
        link_class = link.get("class", "")
        if link_class == "org.yamcs.tctm.ccsds.UdpTmFrameLink":
            link["port"] = spec.tm_port
            link["spacecraftId"] = spec.spacecraft_id
            link["frameLength"] = spec.frame_length
            for vc in link.get("virtualChannels", []):
                vc["maxPacketLength"] = spec.frame_length
                vc["vcId"] = spec.vc_id
        elif link_class == "org.yamcs.tctm.ccsds.UdpTcFrameLink":
            link["port"] = spec.tc_port
            link["spacecraftId"] = spec.spacecraft_id
            link["maxFrameLength"] = spec.frame_length
            for vc in link.get("virtualChannels", []):
                vc["vcId"] = spec.vc_id

    for mdb in config.get("mdb", []):
        if mdb.get("type") == "xtce":
            mdb.setdefault("args", {})["file"] = f"mdb/{spec.name}.xtce.xml"

    for stream in config.get("streamConfig", {}).get("tm", []):
        if isinstance(stream, dict) and stream.get("name") == "tm_realtime":
            stream["rootContainer"] = root_container

    return config


def _validate_specs(specs: list[DeploymentSpec]) -> None:
    """Reject duplicate names/SCIDs and mismatched frame lengths."""
    seen_names: set[str] = set()
    seen_scids: dict[int, str] = {}
    frame_lengths = {spec.frame_length for spec in specs}
    for spec in specs:
        if spec.name in seen_names:
            raise ValueError(f"duplicate deployment name: {spec.name} (use --name)")
        if spec.spacecraft_id in seen_scids:
            raise ValueError(
                f"duplicate spacecraft ID {spec.spacecraft_id}: "
                f"{seen_scids[spec.spacecraft_id]} and {spec.name} — the adapter "
                "routes TM by SCID, so every deployment needs a distinct one"
            )
        seen_names.add(spec.name)
        seen_scids[spec.spacecraft_id] = spec.name
    if len(frame_lengths) > 1:
        raise ValueError(
            f"mismatched {_FRAME_SIZE_CONSTANT} across dictionaries "
            f"({sorted(frame_lengths)}): the adapter uses a single --frame-length"
        )


def generate(
    dictionaries: list[Path],
    output: Path,
    names: list[str] | None = None,
    yamcs_host: str = "127.0.0.1",
    base_port: int = 50000,
    vc_id: int = 1,
    template_dir: Path = DEFAULT_TEMPLATE_DIR,
) -> list[DeploymentSpec]:
    """Generate the full deployment tree; returns the deployment specs."""
    names = names or []
    if len(names) > len(dictionaries):
        raise ValueError("more --name values than dictionaries")

    specs: list[DeploymentSpec] = []
    for index, dictionary in enumerate(dictionaries):
        dictionary = dictionary.resolve()
        if not dictionary.is_file():
            raise ValueError(f"dictionary not found: {dictionary}")
        deployment_name, spacecraft_id, frame_length = _read_dictionary(dictionary)
        name = (
            _sanitize_name(names[index])
            if index < len(names)
            else _default_name(deployment_name, spacecraft_id)
        )
        specs.append(
            DeploymentSpec(
                name=name,
                dictionary=dictionary,
                spacecraft_id=spacecraft_id,
                frame_length=frame_length,
                vc_id=vc_id,
                yamcs_host=yamcs_host,
                tm_port=base_port + 2 * index,
                tc_port=base_port + 2 * index + 1,
            )
        )
    _validate_specs(specs)

    instance_template_path = template_dir / "etc" / "yamcs.fprime-project.yaml"
    server_template_path = template_dir / "etc" / "yamcs.yaml"
    processor_template_path = template_dir / "etc" / "processor.yaml"
    for template_path in (
        instance_template_path,
        server_template_path,
        processor_template_path,
    ):
        if not template_path.is_file():
            raise ValueError(f"template not found: {template_path}")

    with instance_template_path.open() as stream:
        instance_template = yaml.safe_load(stream)
    with server_template_path.open() as stream:
        server_config = yaml.safe_load(stream)

    etc_dir = output / "etc"
    mdb_dir = output / "mdb"
    etc_dir.mkdir(parents=True, exist_ok=True)
    mdb_dir.mkdir(parents=True, exist_ok=True)

    for spec in specs:
        xtce_path = mdb_dir / f"{spec.name}.xtce.xml"
        print(f"[generate] {spec.name}: XTCE from {spec.dictionary}")
        _run_fprime_to_xtce(spec.dictionary, xtce_path)
        root_container = _xtce_root_container(xtce_path)
        instance_config = _render_instance_config(
            instance_template, spec, root_container
        )
        instance_path = etc_dir / f"yamcs.{spec.name}.yaml"
        with instance_path.open("w") as stream:
            yaml.safe_dump(instance_config, stream)
        print(
            f"[generate] {spec.name}: SCID={spec.spacecraft_id} "
            f"TM:{spec.tm_port} TC:{spec.tc_port} rootContainer={root_container}"
        )

    server_config["instances"] = [spec.name for spec in specs]
    with (etc_dir / "yamcs.yaml").open("w") as stream:
        yaml.safe_dump(server_config, stream)
    shutil.copyfile(processor_template_path, etc_dir / "processor.yaml")

    deployments_doc = {
        "deployments": [
            {
                "name": spec.name,
                "spacecraft_id": spec.spacecraft_id,
                "vc_id": spec.vc_id,
                "yamcs_host": spec.yamcs_host,
                "tm_port": spec.tm_port,
                "tc_port": spec.tc_port,
                # Extra key (ignored by the adapter) so launch tooling can start
                # one fprime-yamcs-events bridge per deployment.
                "dictionary": str(spec.dictionary),
            }
            for spec in specs
        ]
    }
    deployments_path = output / "deployments.yaml"
    with deployments_path.open("w") as stream:
        yaml.safe_dump(deployments_doc, stream, sort_keys=False)
    print(f"[generate] wrote {deployments_path}")
    return specs


def parse_args(argv: list[str] | None = None):
    """Parse command-line arguments."""
    parser = argparse.ArgumentParser(
        description="Generate a multi-instance YAMCS deployment tree from F Prime dictionaries",
        formatter_class=argparse.ArgumentDefaultsHelpFormatter,
    )
    parser.add_argument(
        "dictionaries",
        nargs="+",
        type=Path,
        metavar="DICTIONARY",
        help="F Prime *TopologyDictionary.json path(s), one per deployment",
    )
    parser.add_argument(
        "--output",
        type=Path,
        default=DEFAULT_OUTPUT_DIR,
        help="Output directory for the generated YAMCS configuration",
    )
    parser.add_argument(
        "--name",
        dest="names",
        action="append",
        default=None,
        help="Instance name override, repeatable (pairs positionally with dictionaries)",
    )
    parser.add_argument(
        "--yamcs-host", default="127.0.0.1", help="Adapter-facing YAMCS host"
    )
    parser.add_argument(
        "--base-port",
        type=int,
        default=50000,
        help="First UDP port; deployment i gets TM=base+2i, TC=base+2i+1",
    )
    parser.add_argument("--vc-id", type=int, default=1, help="CCSDS virtual channel ID")
    parser.add_argument(
        "--template-dir",
        type=Path,
        default=DEFAULT_TEMPLATE_DIR,
        help="Directory containing the etc/ YAMCS config templates",
    )
    return parser.parse_args(argv)


def main(argv: list[str] | None = None) -> int:
    """Generate the deployment tree and self-check the routing table."""
    args = parse_args(argv)
    try:
        specs = generate(
            dictionaries=args.dictionaries,
            output=args.output,
            names=args.names,
            yamcs_host=args.yamcs_host,
            base_port=args.base_port,
            vc_id=args.vc_id,
            template_dir=args.template_dir,
        )
    except (ValueError, subprocess.CalledProcessError) as exc:
        print(f"[generate] Error: {exc}", file=sys.stderr)
        return 2

    # Self-check: the generated routing table must load through the adapter.
    try:
        from proves_adapter import load_yamcs_deployments
    except ImportError:
        from tools.yamcs.proves_adapter import load_yamcs_deployments
    load_yamcs_deployments(args.output / "deployments.yaml")

    print(
        f"[generate] {len(specs)} deployment(s) ready in {args.output} — start with "
        f"'make yamcs-multi UART_DEVICE=/dev/tty...'"
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())

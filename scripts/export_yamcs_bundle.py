#!/usr/bin/env python3
"""Export the two-file Yamcs runtime bundle for the flashed firmware build."""

from __future__ import annotations

import argparse
import json
import os
import re
import shutil
import sys
import tempfile
from pathlib import Path

KEY_PATTERN = re.compile(
    r'^\s*#\s*define\s+AUTH_DEFAULT_KEY\s+"(?:0[xX])?([0-9a-fA-F]{32})"\s*$',
    re.MULTILINE,
)


def find_dictionary(root: Path) -> Path:
    """Return the only valid topology dictionary beneath root."""
    matches = sorted(root.rglob("*TopologyDictionary.json"))
    if len(matches) != 1:
        raise ValueError(
            f"expected exactly one topology dictionary under {root}; found {len(matches)}"
        )
    try:
        document = json.loads(matches[0].read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as exc:
        raise ValueError(f"invalid topology dictionary {matches[0]}: {exc}") from exc
    if not isinstance(document, dict) or not isinstance(
        document.get("constants"), list
    ):
        raise ValueError(f"topology dictionary has no constants list: {matches[0]}")
    return matches[0]


def extract_auth_key(header: Path) -> str:
    """Extract and normalize the single authentication-key definition."""
    try:
        content = header.read_text(encoding="utf-8")
    except OSError as exc:
        raise ValueError(
            f"unable to read authentication header {header}: {exc}"
        ) from exc
    matches = KEY_PATTERN.findall(content)
    if len(matches) != 1:
        raise ValueError(
            f"expected exactly one 16-byte AUTH_DEFAULT_KEY definition in {header}"
        )
    return matches[0].lower()


def atomic_copy(source: Path, destination: Path) -> None:
    """Copy a public artifact without exposing a partial destination file."""
    destination.parent.mkdir(parents=True, exist_ok=True)
    with (
        source.open("rb") as source_stream,
        tempfile.NamedTemporaryFile(
            mode="wb", dir=destination.parent, delete=False
        ) as destination_stream,
    ):
        shutil.copyfileobj(source_stream, destination_stream)
        temporary = Path(destination_stream.name)
    temporary.chmod(0o644)
    os.replace(temporary, destination)


def atomic_key(key: str, destination: Path) -> None:
    """Write private key material atomically with owner-only permissions."""
    destination.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.NamedTemporaryFile(
        mode="w", encoding="ascii", dir=destination.parent, delete=False
    ) as stream:
        stream.write(f"{key}\n")
        temporary = Path(stream.name)
    temporary.chmod(0o600)
    os.replace(temporary, destination)


def export_bundle(dictionary_root: Path, auth_header: Path, output_dir: Path) -> None:
    """Validate and export the stable two-file Yamcs runtime bundle."""
    dictionary = find_dictionary(dictionary_root)
    key = extract_auth_key(auth_header)
    output_dir = output_dir.expanduser().resolve()
    atomic_copy(dictionary, output_dir / "fprime-dictionary.json")
    atomic_key(key, output_dir / "auth-key.hex")
    print(f"Exported Yamcs runtime bundle to {output_dir}")
    print("Use this bundle only with the firmware build that produced it.")


def parse_args() -> argparse.Namespace:
    """Parse bundle source and destination paths."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--dictionary-root", type=Path, default=Path("build-artifacts"))
    parser.add_argument(
        "--auth-header",
        type=Path,
        default=Path(
            "PROVESFlightControllerReference/Components/TcSecurityDeframer/"
            "AuthDefaultKey.h"
        ),
    )
    parser.add_argument("--output-dir", type=Path, required=True)
    return parser.parse_args()


def main() -> int:
    """Run the command-line exporter."""
    args = parse_args()
    try:
        export_bundle(args.dictionary_root, args.auth_header, args.output_dir)
    except ValueError as exc:
        print(f"Error: {exc}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

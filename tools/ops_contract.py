#!/usr/bin/env python3
"""Check the ops contract (ops-contract/manifest.json) against a built dictionary.

The manifest lists every dictionary item the mission ground tools depend on.
Its keys are written by hand; its values are generated from the dictionary.
`check` regenerates the values and fails on any difference. `--write` stores
the regenerated values, which is how a pull request records a contract change
(see ops-contract/README.md). Add a name with an empty object as its value and
run `--write` to fill it in.
"""

import argparse
import difflib
import json
import sys
from pathlib import Path

KINDS = ("commands", "events", "channels", "parameters", "packets")


def wire_type(t: dict, types: dict) -> str:
    """Describe a type by its byte layout and values, not its name.

    A renamed type with the same layout (Svc.CmdSequencer.BlockState to
    Svc.BlockState) keeps the contract; a new enum value or a resized string
    breaks it.
    """
    kind = t["kind"]
    if kind == "integer":
        return f"{'I' if t['signed'] else 'U'}{t['size']}"
    if kind == "float":
        return f"F{t['size']}"
    if kind == "bool":
        return "bool"
    if kind == "string":
        return f"string[{t.get('size', '')}]"
    definition = types[t["name"]]
    defined = definition["kind"]
    if defined == "alias":
        return wire_type(definition["underlyingType"], types)
    if defined == "enum":
        constants = ", ".join(
            f"{c['name']}={c['value']}" for c in definition["enumeratedConstants"]
        )
        return (
            f"enum {wire_type(definition['representationType'], types)} {{{constants}}}"
        )
    if defined == "array":
        return f"{wire_type(definition['elementType'], types)}[{definition['size']}]"
    if defined == "struct":
        members = sorted(definition["members"].items(), key=lambda m: m[1]["index"])
        return (
            "{"
            + ", ".join(f"{name}: {wire_type(m['type'], types)}" for name, m in members)
            + "}"
        )
    raise ValueError(f"unknown type kind {defined} for {t['name']}")


def generate(dictionary: dict, names: dict[str, list[str]]) -> dict:
    types = {t["qualifiedName"]: t for t in dictionary["typeDefinitions"]}

    def args(item: dict) -> list[str]:
        return [wire_type(p["type"], types) for p in item["formalParams"]]

    by_name = {
        "commands": {c["name"]: c for c in dictionary["commands"]},
        "events": {e["name"]: e for e in dictionary["events"]},
        "channels": {c["name"]: c for c in dictionary["telemetryChannels"]},
        "parameters": {p["name"]: p for p in dictionary["parameters"]},
        "packets": {
            p["name"]: p
            for packet_set in dictionary["telemetryPacketSets"]
            for p in packet_set["members"]
        },
    }
    describe = {
        "commands": lambda c: {"opcode": f"0x{c['opcode']:08x}", "args": args(c)},
        "events": lambda e: {
            "id": f"0x{e['id']:08x}",
            "severity": e["severity"],
            "format": e["format"],
            "args": args(e),
        },
        "channels": lambda c: {
            "id": f"0x{c['id']:08x}",
            "type": wire_type(c["type"], types),
        },
        "parameters": lambda p: {
            "id": f"0x{p['id']:08x}",
            "type": wire_type(p["type"], types),
            "default": p.get("default"),
        },
        "packets": lambda p: {
            "id": p["id"],
            "group": p["group"],
            "members": [
                f"{m}: {wire_type(by_name['channels'][m]['type'], types)}"
                for m in p["members"]
            ],
        },
    }
    return {
        kind: {
            name: describe[kind](by_name[kind][name])
            if name in by_name[kind]
            else "MISSING"
            for name in sorted(names[kind])
        }
        for kind in KINDS
    }


def render(manifest: dict) -> str:
    return json.dumps(manifest, indent=2) + "\n"


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("dictionary", type=Path, help="built *TopologyDictionary.json")
    parser.add_argument("manifest", type=Path, help="ops-contract/manifest.json")
    parser.add_argument(
        "--write", action="store_true", help="store the regenerated values"
    )
    options = parser.parse_args()

    committed_text = options.manifest.read_text(encoding="utf-8")
    committed = json.loads(committed_text)
    names = {kind: list(committed.get(kind, {})) for kind in KINDS}
    manifest = generate(
        json.loads(options.dictionary.read_text(encoding="utf-8")), names
    )
    generated = render(manifest)
    missing = [
        name
        for kind in KINDS
        for name, value in manifest[kind].items()
        if value == "MISSING"
    ]
    for name in missing:
        print(f"not in the dictionary: {name}", file=sys.stderr)

    if options.write:
        options.manifest.write_text(generated, encoding="utf-8")
        return 1 if missing else 0
    if generated == committed_text and not missing:
        print(f"ops contract holds: {sum(map(len, names.values()))} items")
        return 0
    sys.stdout.writelines(
        difflib.unified_diff(
            committed_text.splitlines(keepends=True),
            generated.splitlines(keepends=True),
            f"{options.manifest} (committed)",
            f"{options.manifest} (from {options.dictionary.name})",
        )
    )
    print(
        "\nThe dictionary no longer matches the ops contract. If the change is intended, "
        "run `make ops-contract` and follow ops-contract/README.md.",
        file=sys.stderr,
    )
    return 1


if __name__ == "__main__":
    sys.exit(main())

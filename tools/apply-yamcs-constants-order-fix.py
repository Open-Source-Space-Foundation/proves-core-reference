#!/usr/bin/env python3
"""Fix fprime-yamcs get_dictionary_constants to return constants in request order.

fprime-yamcs 0.1.3's get_dictionary_constants() filters the dictionary's
"constants" array with a list comprehension, so results come back in
*dictionary file* order, not the order the caller asked for. The caller
assumes constants[0] == TmFrameFixedSize and constants[1] == SpacecraftId.
F Prime v4.2.2 dictionaries list ComCfg.SpacecraftId before
ComCfg.TmFrameFixedSize, which silently configures YAMCS's UdpTmFrameLink
with frameLength=67 and spacecraftId=248 — every TM frame is then rejected
as invalid and no telemetry, parameters, or events ever reach YAMCS.

Applied by `make fprime-venv`. Exits non-zero if the expected source is
missing and the fix marker is absent, so a package upgrade cannot silently
reintroduce the bug (do not fail-open on string mismatch).
"""

import sys
from pathlib import Path

ORIGINAL = """    found_constants = [
        constant["value"] for constant in constants_data if constant.get("qualifiedName", "") in constants
    ]
"""

REPLACEMENT = """    constants_by_name = {
        constant.get("qualifiedName", ""): constant["value"] for constant in constants_data
    }
    found_constants = [constants_by_name[name] for name in constants if name in constants_by_name]
"""

MARKER = "constants_by_name"


def main() -> int:
    if len(sys.argv) != 2:
        print(
            f"usage: {sys.argv[0]} <path-to-fprime_yamcs/__main__.py>", file=sys.stderr
        )
        return 2
    target = Path(sys.argv[1])
    source = target.read_text()

    if MARKER in source:
        print("⚠ fprime-yamcs constants-order fix already applied")
        return 0
    if ORIGINAL not in source:
        print(
            f"❌ Error: expected get_dictionary_constants source not found in {target}; "
            "fprime-yamcs may have changed — re-verify the constants ordering bug "
            "before removing this fixer.",
            file=sys.stderr,
        )
        return 1

    target.write_text(source.replace(ORIGINAL, REPLACEMENT, 1))
    print("✓ Applied fprime-yamcs constants-order fix")
    return 0


if __name__ == "__main__":
    sys.exit(main())

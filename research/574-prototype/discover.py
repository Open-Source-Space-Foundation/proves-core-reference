"""Shared discovery for both prototypes.

Yields every file in every component docs/ folder that contains an sdd.md,
mapped to components/<ComponentName>/<path inside docs/>.

Each source root carries an edit base. None means "this repo": the edit path is
relative to repo_url + edit_uri. A URL means another repo (the git-submodule
case): the edit link is absolute.

Set PROTO_EXTRA_ROOT (and optionally PROTO_EXTRA_EDIT_BASE) to add a second,
submodule-shaped source root for testing.
"""

import os
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
SOURCE_ROOTS = [(REPO / "PROVESFlightControllerReference", None)]
if os.environ.get("PROTO_EXTRA_ROOT"):
    SOURCE_ROOTS.append(
        (Path(os.environ["PROTO_EXTRA_ROOT"]), os.environ.get("PROTO_EXTRA_EDIT_BASE"))
    )


def component_doc_files():
    """Yield (site_path, abs_source_path, edit_target, is_absolute_url)."""
    for root, edit_base in SOURCE_ROOTS:
        sdds = sorted(root.glob("**/docs/sdd.md"))
        if not sdds:
            # An uninitialised submodule is an empty directory; fail loudly.
            raise SystemExit(f"no */docs/sdd.md under source root {root}")
        for sdd in sdds:
            docs = sdd.parent
            name = docs.parent.name
            for f in sorted(p for p in docs.rglob("*") if p.is_file()):
                site_path = f"components/{name}/{f.relative_to(docs).as_posix()}"
                if edit_base:
                    yield site_path, f, f"{edit_base}/{f.relative_to(root).as_posix()}", True
                else:
                    yield site_path, f, f"../{f.relative_to(REPO).as_posix()}", False

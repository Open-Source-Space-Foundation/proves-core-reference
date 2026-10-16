"""Derive the two prototype configs from the real mkdocs.yml.

Run from the repo root: python3 research/574-prototype/make_configs.py
"""

import re
from pathlib import Path

HERE = Path(__file__).parent
base = (HERE / "../../mkdocs.yml").read_text()

# nav: components/X.md -> components/X/sdd.md ; safe-mode-fsm.md lives in ModeManager/docs/
base = base.replace("components/safe-mode-fsm.md", "components/ModeManager/safe-mode-fsm")
base = re.sub(r"components/(\w+)\.md", r"components/\1/sdd.md", base)
base = base.replace("components/ModeManager/safe-mode-fsm", "components/ModeManager/safe-mode-fsm.md")
base = base.replace(
    "          - Veml6031 Manager: components/Veml6031Manager/sdd.md\n",
    "          - Veml6031 Manager: components/Veml6031Manager/sdd.md\n"
    "          - Pico Temp Manager: components/PicoTempManager/sdd.md\n",
)

# Paths in a config file resolve relative to the config file's directory.
base = base.replace(
    "docs_dir: docs-site",
    # A copy of docs-site/ with components/ deleted and index.md links rewritten
    # to components/<Name>/sdd.md (see research/574-docs-generation-tooling.md, Method).
    # exclude_docs: /components/ does NOT work: MkDocs re-applies exclude_docs to
    # files added in on_files (mkdocs/commands/build.py, set_exclusions after on_files).
    "docs_dir: /tmp/mkres/docs-site-nocomponents\n"
    "edit_uri: edit/main/docs-site/",
)
base = base.replace("    - content.code.copy\n", "    - content.code.copy\n    - content.action.edit\n")
watch = "\nwatch:\n  - ../../PROVESFlightControllerReference\n"

genfiles = base.replace(
    "  - tags\n",
    "  - tags\n  - gen-files:\n      scripts:\n        - gen_components.py\n",
)
(HERE / "mkdocs-genfiles.yml").write_text(genfiles + watch)
(HERE / "mkdocs-hooks.yml").write_text(base + "\nhooks:\n  - hooks.py\n" + watch)

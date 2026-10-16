# Prototype A: mkdocs-gen-files script.
import sys
from pathlib import Path

import mkdocs_gen_files

sys.path.insert(0, str(Path(__file__).parent))
from discover import component_doc_files  # noqa: E402

for site_path, src, edit_target, _absolute in component_doc_files():
    with mkdocs_gen_files.open(site_path, "wb") as fd:
        fd.write(src.read_bytes())
    # urljoin(repo_url + edit_uri, edit_target): a relative path resolves against
    # edit/main/docs-site/, an absolute URL (submodule repo) passes through.
    # Only pages consume it; calling it for assets logs a "went unused" WARNING.
    if site_path.endswith(".md"):
        mkdocs_gen_files.set_edit_path(site_path, edit_target)

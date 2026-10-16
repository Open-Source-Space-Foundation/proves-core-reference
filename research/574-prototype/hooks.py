# Prototype B: MkDocs `hooks:` file (no extra dependency).
import sys
from pathlib import Path

from mkdocs.structure.files import File

sys.path.insert(0, str(Path(__file__).parent))
from discover import component_doc_files  # noqa: E402

_absolute_edit_urls = {}


def on_files(files, config):
    _absolute_edit_urls.clear()
    for site_path, src, edit_target, absolute in component_doc_files():
        # Virtual file whose content is read straight from the source path (MkDocs >= 1.6).
        f = File.generated(config, site_path, abs_src_path=str(src))
        if absolute:
            # MkDocs builds edit_uri + File.edit_uri, so an absolute URL can't go
            # through File.edit_uri; override page.edit_url in on_page_markdown.
            _absolute_edit_urls[site_path] = edit_target
        else:
            # Appended to repo_url + edit_uri (edit_uri = edit/main/docs-site/).
            f.edit_uri = edit_target
        files.append(f)
    return files


def on_page_markdown(markdown, page, config, files):
    url = _absolute_edit_urls.get(page.file.src_uri)
    if url:
        page.edit_url = url
    return markdown

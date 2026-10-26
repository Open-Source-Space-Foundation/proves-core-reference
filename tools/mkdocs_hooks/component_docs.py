"""MkDocs hook that publishes every component's docs/ folder under components/<Name>/.

Sources are declared in mkdocs.yml under extra.component_sources. The hook
discovers components, adds their files to the site, fills the empty
"Component SDDs" nav entry from sdd.md front matter, rewrites links between
component docs folders, and gives each generated page an edit link into its
source repository.

Run directly to list the declared sources that are git submodules (used by
`make docs-deps`):

    python tools/mkdocs_hooks/component_docs.py --submodule-sources mkdocs.yml
"""

from __future__ import annotations

import logging
import os
import posixpath
import re
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import Mapping
from urllib.parse import unquote, urlsplit

from markdown import Extension, Markdown
from markdown.treeprocessors import Treeprocessor
from mkdocs.config.defaults import MkDocsConfig
from mkdocs.exceptions import PluginError
from mkdocs.structure.files import File, Files
from mkdocs.structure.pages import Page as SitePage
from mkdocs.utils import is_markdown_file, meta
from mkdocs.utils.yaml import yaml_load

log = logging.getLogger("mkdocs.hooks.component_docs")

MENU_TITLE = "Component SDDs"
OTHER = "Other"
SDD = "sdd.md"
SITE_DIR = "components"


@dataclass(frozen=True)
class Source:
    """A declared component source folder and the edit URL prefix for its files."""

    path: Path
    edit_uri: str


def rel(path: Path | str, repo_root: Path) -> str:
    """`path` relative to the repo root, for messages."""
    return Path(os.path.relpath(path, repo_root)).as_posix()


@dataclass(frozen=True)
class Page:
    """A Markdown file to publish and its menu title."""

    path: Path
    title: str


@dataclass(frozen=True)
class Component:
    """One folder with a docs/ child, and the files it publishes."""

    name: str
    source: Source
    docs_dir: Path
    sdd: Page
    category: object
    pages: tuple[Page, ...]
    assets: tuple[Path, ...]

    @property
    def files(self) -> tuple[Path, ...]:
        """Every file to publish: the SDD, then other pages, then assets."""
        return (self.sdd.path, *(p.path for p in self.pages), *self.assets)

    def src_uri(self, path: Path) -> str:
        """Site path for `path`: components/<Name>/<path inside docs/>."""
        return posixpath.join(
            SITE_DIR, self.name, path.relative_to(self.docs_dir).as_posix()
        )

    def edit_url(self, path: Path) -> str:
        """Edit link for `path`: the source edit_uri plus the path inside the source."""
        return self.source.edit_uri + path.relative_to(self.source.path).as_posix()


Registry = Mapping[str, Component]


def parse_sources(raw: object, repo_root: Path) -> tuple[list[Source], list[str]]:
    """Parse extra.component_sources into sources, plus an error per malformed entry."""
    if not isinstance(raw, list) or not raw:
        return [], [
            "extra.component_sources is missing or empty in mkdocs.yml; "
            "add at least one entry with 'path' and 'edit_uri'"
        ]
    sources: list[Source] = []
    errors: list[str] = []
    for entry in raw:
        if (
            not isinstance(entry, dict)
            or not entry.get("path")
            or not entry.get("edit_uri")
        ):
            errors.append(
                f"extra.component_sources entry {entry!r} needs both 'path' and 'edit_uri'"
            )
            continue
        edit_uri = str(entry["edit_uri"])
        sources.append(
            Source(
                repo_root / str(entry["path"]).rstrip("/"),
                edit_uri if edit_uri.endswith("/") else edit_uri + "/",
            )
        )
    return sources, errors


_H1 = re.compile(r"^#\s+(.+?)\s*#*\s*$", re.MULTILINE)


def read_page(path: Path, title: str | None = None) -> tuple[Page, dict]:
    """`path` and its front matter. The title is front matter `title:`, else
    `title`, else the first `# ` heading, else the file name."""
    markdown, data = meta.get_data(path.read_text(encoding="utf-8"))
    if data.get("title"):
        title = str(data["title"])
    elif title is None:
        heading = _H1.search(markdown)
        title = heading.group(1) if heading else path.stem
    return Page(path, title), data


def discover(source: Source, repo_root: Path) -> tuple[list[Component], list[str]]:
    """Find every folder below `source` that has a docs/ child."""
    declared = rel(source.path, repo_root)
    if not source.path.is_dir():
        return [], [
            f"component source path '{declared}' does not exist or is not a directory"
        ]
    components: list[Component] = []
    errors: list[str] = []
    folders_with_docs = 0
    for dirpath, dirnames, _ in os.walk(source.path):
        dirnames[:] = sorted(d for d in dirnames if not d.startswith("."))
        if "docs" not in dirnames:
            continue
        dirnames.remove("docs")
        folder = Path(dirpath)
        if folder == source.path:
            continue
        folders_with_docs += 1
        docs_dir = folder / "docs"
        files = sorted(p for p in docs_dir.rglob("*") if p.is_file())
        if docs_dir / SDD not in files:
            errors.append(f"{rel(docs_dir, repo_root)} has no {SDD}")
            continue
        sdd, data = read_page(docs_dir / SDD, folder.name)
        components.append(
            Component(
                name=folder.name,
                source=source,
                docs_dir=docs_dir,
                sdd=sdd,
                category=data.get("category"),
                pages=tuple(
                    read_page(p)[0]
                    for p in files
                    if p != sdd.path and is_markdown_file(p.name)
                ),
                assets=tuple(p for p in files if not is_markdown_file(p.name)),
            )
        )
    if folders_with_docs == 0:
        errors.append(
            f"component source '{declared}' has no components (no folder with a "
            f"docs/ child); if it is a git submodule, run: "
            f"git submodule update --init {declared}"
        )
    return components, errors


def build_registry(
    sources: list[Source], repo_root: Path
) -> tuple[dict[str, Component], list[str]]:
    """Discover every source and index components by name, reporting duplicates."""
    registry: dict[str, Component] = {}
    errors: list[str] = []
    for source in sources:
        components, found = discover(source, repo_root)
        errors += found
        for component in components:
            first = registry.get(component.name)
            if first is None:
                registry[component.name] = component
                continue
            errors.append(
                f"two components are named '{component.name}': "
                f"{rel(first.docs_dir.parent, repo_root)} and "
                f"{rel(component.docs_dir.parent, repo_root)}; "
                f"component folder names must be unique across all sources"
            )
    return registry, errors


def _menu_entry(component: Component) -> dict:
    """A single nav entry, or a section headed by the SDD when there are more pages."""
    sdd = component.src_uri(component.sdd.path)
    pages = sorted(component.pages, key=lambda p: p.title.casefold())
    section = [sdd, *({p.title: component.src_uri(p.path)} for p in pages)]
    return {component.sdd.title: section if pages else sdd}


def build_menu(
    registry: Registry, categories: list[str]
) -> tuple[list[dict], list[str]]:
    """Nav entries grouped by `category:` in `categories` order, plus INFO notes."""
    groups: dict[str, list[tuple[str, dict]]] = {c: [] for c in categories}
    groups[OTHER] = []
    notes: list[str] = []
    for component in registry.values():
        category = component.category
        if not isinstance(category, str) or category not in categories:
            notes.append(
                f"{component.name}: category {category!r} is not one of "
                f"{', '.join(categories)}; listing it under {OTHER}"
            )
            category = OTHER
        groups[category].append((component.sdd.title, _menu_entry(component)))
    menu = [
        {name: [entry for _, entry in sorted(entries, key=lambda e: e[0].casefold())]}
        for name, entries in groups.items()
        if entries
    ]
    return menu, notes


def fill_menu(nav: list | None, menu: list[dict]) -> list | None:
    """Replace the value of the top-level `Component SDDs` nav entry with `menu`."""
    for i, item in enumerate(nav or []):
        if isinstance(item, dict) and MENU_TITLE in item:
            return [*nav[:i], {MENU_TITLE: menu}, *nav[i + 1 :]]
    log.warning(f"mkdocs.yml nav has no '{MENU_TITLE}' entry for the hook to fill")
    return nav


def rewrite_link(
    target: str, page: Path, page_uri: str, by_docs_dir: Mapping[Path, Component]
) -> str | None:
    """The href to use for `target` on `page`, or None if it leaves component docs.

    Links into another component's docs/ folder are rewritten to that file's
    place under components/<Name>/, keeping the #anchor.
    """
    parts = urlsplit(target)
    if parts.scheme or parts.netloc or not parts.path or parts.path.startswith("/"):
        return target
    resolved = Path(os.path.normpath(page.parent / unquote(parts.path)))
    owner = next((by_docs_dir[d] for d in resolved.parents if d in by_docs_dir), None)
    if owner is None:
        return None
    if owner.docs_dir in page.parents:
        return target
    href = posixpath.relpath(owner.src_uri(resolved), posixpath.dirname(page_uri))
    return href + (f"#{parts.fragment}" if parts.fragment else "")


class _LinkRewriter(Treeprocessor):
    """Markdown tree pass that applies `rewrite_link` to component pages."""

    def __init__(self, docs: _ComponentDocs) -> None:
        super().__init__()
        self.docs = docs

    def run(self, root) -> None:
        """Rewrite or flag every relative link on the current component page."""
        current = self.docs.current
        if current is None:
            return
        path, page_uri = current
        for element in root.iter():
            attribute = {"a": "href", "img": "src"}.get(element.tag)
            if attribute is None:
                continue
            target = element.get(attribute)
            if not target:
                continue
            href = rewrite_link(target, path, page_uri, self.docs.by_docs_dir)
            if href is None:
                outside = path.parent / unquote(urlsplit(target).path)
                log.warning(
                    f"{rel(path, self.docs.repo_root)} links to "
                    f"{rel(outside, self.docs.repo_root)} outside "
                    f"component docs; use a GitHub URL"
                )
            elif href != target:
                element.set(attribute, href)


class _LinkExtension(Extension):
    """Registers `_LinkRewriter` on every page's Markdown instance."""

    def __init__(self, docs: _ComponentDocs) -> None:
        super().__init__()
        self.docs = docs

    def extendMarkdown(self, md: Markdown) -> None:
        """Register the link pass."""
        # After inline parsing (20), before MkDocs validates relative links (0).
        md.treeprocessors.register(_LinkRewriter(self.docs), "component_links", 10)


class _ComponentDocs:
    """State for one build: the registry, its errors, and the page being rendered."""

    def __init__(self, registry: Registry, repo_root: Path, errors: list[str]) -> None:
        self.registry = registry
        self.repo_root = repo_root
        self.errors = errors
        self.by_docs_dir = {c.docs_dir: c for c in registry.values()}
        self.by_src_uri: dict[str, tuple[Component, Path]] = {}
        self.current: tuple[Path, str] | None = None


_docs: _ComponentDocs | None = None


def on_config(config: MkDocsConfig) -> MkDocsConfig:
    """Validate sources, discover components, watch sources, and fill the menu.

    Errors are kept for on_files, which raises them with any collisions.
    """
    global _docs
    repo_root = Path(config.config_file_path).parent
    sources, errors = parse_sources(config.extra.get("component_sources"), repo_root)
    registry, found = build_registry(sources, repo_root)
    errors += found
    config.watch.extend(str(s.path) for s in sources)
    menu, notes = build_menu(
        registry, list(config.extra.get("component_categories") or [])
    )
    for note in notes:
        log.info(note)
    config.nav = fill_menu(config.nav, menu)

    _docs = _ComponentDocs(registry, repo_root, errors)
    config.markdown_extensions.append(_LinkExtension(_docs))
    return config


def on_files(files: Files, config: MkDocsConfig) -> Files:
    """Add every component file to the site, then raise every error found."""
    assert _docs is not None
    by_dest = {f.dest_uri: f for f in files}
    errors = _docs.errors
    for component in _docs.registry.values():
        for path in component.files:
            file = File.generated(
                config, component.src_uri(path), abs_src_path=str(path)
            )
            if path == component.sdd.path:
                # sdd.md keeps its name so links to it resolve, but serves as
                # the folder index, which navigation.indexes also requires.
                file.dest_uri = posixpath.join(SITE_DIR, component.name, "index.html")
                file.name = "index"
            clash = files.get_file_from_path(file.src_uri) or by_dest.get(file.dest_uri)
            if clash is not None:
                other = clash.abs_src_path
                errors.append(
                    f"{rel(path, _docs.repo_root)} would be published as "
                    f"{file.dest_uri}, which collides with "
                    f"{rel(other, _docs.repo_root) if other else clash.src_uri}"
                )
                continue
            files.append(file)
            by_dest[file.dest_uri] = file
            _docs.by_src_uri[file.src_uri] = (component, path)
    if errors:
        lines = "\n".join(f"  - {e}" for e in errors)
        raise PluginError(f"component docs: {len(errors)} problem(s):\n{lines}")
    return files


def on_pre_page(page: SitePage, config: MkDocsConfig, files: Files) -> SitePage:
    """Point a component page's edit link at its source file."""
    assert _docs is not None
    found = _docs.by_src_uri.get(page.file.src_uri)
    if found is not None:
        component, path = found
        page.edit_url = component.edit_url(path)
    return page


def on_page_markdown(
    markdown: str, page: SitePage, config: MkDocsConfig, files: Files
) -> str:
    """Record the component page about to render for `_LinkRewriter`, which
    runs inside Markdown and cannot reach the Page."""
    assert _docs is not None
    found = _docs.by_src_uri.get(page.file.src_uri)
    _docs.current = (found[1], page.file.src_uri) if found else None
    return markdown


def submodule_sources(mkdocs_yml: Path) -> list[str]:
    """The extra.component_sources paths in `mkdocs_yml` that are git submodules
    listed in the .gitmodules beside it."""
    with mkdocs_yml.open(encoding="utf-8") as stream:
        extra = yaml_load(stream).get("extra") or {}
    gitmodules = mkdocs_yml.parent / ".gitmodules"
    text = gitmodules.read_text(encoding="utf-8") if gitmodules.is_file() else ""
    submodules = set(re.findall(r"^\s*path\s*=\s*(.+?)\s*$", text, re.MULTILINE))
    sources, _ = parse_sources(extra.get("component_sources"), mkdocs_yml.parent)
    declared = [rel(s.path, mkdocs_yml.parent) for s in sources]
    return [path for path in declared if path in submodules]


if __name__ == "__main__":
    if len(sys.argv) != 3 or sys.argv[1] != "--submodule-sources":
        sys.exit("usage: component_docs.py --submodule-sources <mkdocs.yml>")
    print("\n".join(submodule_sources(Path(sys.argv[2]))))

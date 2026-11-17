"""Tests for the component docs MkDocs hook.

Most tests build a small throwaway site with the real `mkdocs build --strict`
so the hook runs end to end. Run with: make docs-test
"""

import subprocess
import sys
from dataclasses import dataclass
from pathlib import Path

import component_docs
import yaml
from mkdocs.config import load_config

HOOK = Path(component_docs.__file__)

SOURCE = {"path": "src", "edit_uri": "https://example.com/repo/edit/main/src/"}


def sdd(title="Foo", category="Core", body=""):
    """An sdd.md with front matter and an optional body."""
    return f"---\ntitle: {title}\ncategory: {category}\n---\n\n# Components::{title}\n\n{body}\n"


@dataclass
class Build:
    """Result of one `mkdocs build --strict` run on a fixture site."""

    returncode: int
    log: str
    site: Path

    def html(self, path):
        """Rendered HTML for `path` under the site directory."""
        return (self.site / path).read_text(encoding="utf-8")


def build_site(root: Path, files: dict, extra: dict | None = None) -> Build:
    """Write `files` under `root` with a minimal mkdocs.yml and build it."""
    tree = {"docs-site/index.md": "# Home\n", **files}
    for rel, content in tree.items():
        path = root / rel
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(content, encoding="utf-8")
    config = {
        "site_name": "Test",
        "repo_url": "https://example.com/repo",
        "edit_uri": "edit/main/docs-site/",
        "docs_dir": "docs-site",
        "hooks": [str(HOOK)],
        "nav": [{"Home": "index.md"}, {"Component SDDs": []}],
        "extra": {
            "component_categories": ["Core", "Driver"],
            "component_sources": [SOURCE],
            **(extra or {}),
        },
    }
    (root / "mkdocs.yml").write_text(yaml.safe_dump(config), encoding="utf-8")
    site = root / "site"
    result = subprocess.run(
        [sys.executable, "-m", "mkdocs", "build", "--strict"]
        + ["-f", str(root / "mkdocs.yml"), "-d", str(site)],
        capture_output=True,
        text=True,
    )
    return Build(result.returncode, result.stdout + result.stderr, site)


def assert_aborted_with(build: Build, *messages: str):
    """The build failed with one error that contains every message."""
    assert build.returncode != 0, build.log
    assert build.log.count("component docs:") == 1, build.log
    for message in messages:
        assert message in build.log, build.log


def test_missing_component_sources_fails(tmp_path):
    """Failure 1: no component_sources aborts the build."""
    build = build_site(tmp_path, {}, extra={"component_sources": []})
    assert_aborted_with(build, "extra.component_sources is missing or empty")


def test_source_entry_without_edit_uri_fails_naming_the_entry(tmp_path):
    """Failure 1: an entry without edit_uri is named in the error."""
    build = build_site(
        tmp_path,
        {"src/Foo/docs/sdd.md": sdd()},
        extra={"component_sources": [{"path": "src"}]},
    )
    assert_aborted_with(
        build,
        "extra.component_sources entry {'path': 'src'} needs both 'path' and 'edit_uri'",
    )


def test_source_path_that_does_not_exist_fails(tmp_path):
    """Failure 2: a missing source path is named in the error."""
    build = build_site(
        tmp_path,
        {},
        extra={"component_sources": [{**SOURCE, "path": "missing"}]},
    )
    assert_aborted_with(
        build, "component source path 'missing' does not exist or is not a directory"
    )


def test_source_without_components_suggests_submodule_init(tmp_path):
    """Failure 3: an empty source names the submodule init command."""
    build = build_site(tmp_path, {"src/Foo/Foo.fpp": "module Foo {}\n"})
    assert_aborted_with(
        build,
        "component source 'src' has no components",
        "git submodule update --init src",
    )


def test_duplicate_component_names_fail_naming_both_paths(tmp_path):
    """Failure 4: two components with one name, within a source."""
    build = build_site(
        tmp_path,
        {"src/A/Foo/docs/sdd.md": sdd(), "src/B/Foo/docs/sdd.md": sdd()},
    )
    assert_aborted_with(
        build, "two components are named 'Foo': src/A/Foo and src/B/Foo"
    )


def test_duplicate_names_across_sources_fail(tmp_path):
    """Failure 4: two components with one name, across sources."""
    other = {"path": "lib", "edit_uri": "https://example.com/lib/edit/main/"}
    build = build_site(
        tmp_path,
        {"src/Foo/docs/sdd.md": sdd(), "lib/Foo/docs/sdd.md": sdd()},
        extra={"component_sources": [SOURCE, other]},
    )
    assert_aborted_with(build, "two components are named 'Foo': src/Foo and lib/Foo")


def test_docs_folder_without_sdd_fails_naming_the_folder(tmp_path):
    """Failure 5: a docs/ folder without sdd.md."""
    build = build_site(tmp_path, {"src/Foo/docs/notes.md": "# Notes\n"})
    assert_aborted_with(build, "src/Foo/docs has no sdd.md")


def test_generated_page_colliding_with_hand_written_page_fails(tmp_path):
    """Failure 6: a generated page collides with a docs-site page."""
    build = build_site(
        tmp_path,
        {
            "src/Foo/docs/sdd.md": sdd(),
            "docs-site/components/Foo.md": "# Old copy\n",
        },
    )
    assert_aborted_with(
        build,
        "src/Foo/docs/sdd.md would be published as components/Foo/index.html, "
        "which collides with docs-site/components/Foo.md",
    )


def test_two_failures_are_reported_in_one_error(tmp_path):
    """Every problem found while configuring is raised together."""
    build = build_site(
        tmp_path,
        {
            "src/A/Foo/docs/sdd.md": sdd(),
            "src/B/Foo/docs/sdd.md": sdd(),
            "src/Bar/docs/notes.md": "# Notes\n",
        },
    )
    assert_aborted_with(
        build,
        "component docs: 2 problem(s)",
        "src/Bar/docs has no sdd.md",
        "two components are named 'Foo'",
    )


def test_missing_sdd_and_hand_written_collision_are_reported_together(tmp_path):
    """Failures found while configuring and while adding files share one error."""
    build = build_site(
        tmp_path,
        {
            "src/Foo/docs/sdd.md": sdd(),
            "src/Bar/docs/notes.md": "# Notes\n",
            "docs-site/components/Foo.md": "# Old copy\n",
        },
    )
    assert_aborted_with(
        build,
        "component docs: 2 problem(s)",
        "src/Bar/docs has no sdd.md",
        "src/Foo/docs/sdd.md would be published as components/Foo/index.html, "
        "which collides with docs-site/components/Foo.md",
    )


def test_unknown_category_logs_info_and_lands_in_other(tmp_path):
    """An unknown category logs INFO, passes --strict, and lists under Other."""
    build = build_site(
        tmp_path,
        {
            "src/Foo/docs/sdd.md": sdd(category="Sensors"),
            "src/Bar/docs/sdd.md": sdd(title="Bar"),
        },
    )
    assert build.returncode == 0, build.log
    info = [line for line in build.log.splitlines() if "category 'Sensors'" in line]
    assert len(info) == 1 and info[0].startswith("INFO"), build.log
    assert "Foo:" in info[0] and "Other" in info[0]
    home = build.html("index.html")
    assert home.index("Core") < home.index("Other")


def test_link_to_a_code_file_warns_and_fails_strict(tmp_path):
    """A link out of docs/ to a code file warns with the fix."""
    build = build_site(
        tmp_path,
        {
            "src/Foo/Foo.fpp": "module Foo {}\n",
            "src/Foo/docs/sdd.md": sdd(body="See [the model](../Foo.fpp)."),
        },
    )
    assert build.returncode != 0, build.log
    assert (
        "WARNING -  src/Foo/docs/sdd.md links to src/Foo/Foo.fpp outside "
        "component docs; use a GitHub URL"
    ) in build.log


def test_link_into_another_components_docs_is_rewritten(tmp_path):
    """Links into another component's docs/ point at its site pages."""
    build = build_site(
        tmp_path,
        {
            "src/Foo/docs/sdd.md": sdd(
                body="See [Bar ports](../../Bar/docs/sdd.md#ports) and "
                "[Bar usage](../../Bar/docs/usage.md)."
            ),
            "src/Bar/docs/sdd.md": sdd(title="Bar", body="## Ports\n"),
            "src/Bar/docs/usage.md": "# Usage\n",
        },
    )
    assert build.returncode == 0, build.log
    page = build.html("components/Foo/index.html")
    assert 'href="../Bar/#ports"' in page
    assert 'href="../Bar/usage/"' in page


def test_sdd_is_served_at_the_component_folder_url(tmp_path):
    """sdd.md is served at components/<Name>/ and links to it resolve."""
    build = build_site(
        tmp_path,
        {
            "src/Drv/Foo/docs/sdd.md": sdd(),
            "src/Drv/Foo/docs/extra.md": "# Extra\n\nBack to [the SDD](sdd.md).\n",
        },
    )
    assert build.returncode == 0, build.log
    assert (build.site / "components/Foo/index.html").is_file()
    assert not (build.site / "components/Foo/sdd").exists()
    assert 'href="../"' in build.html("components/Foo/extra/index.html")


def test_assets_are_published_next_to_their_pages(tmp_path):
    """Images and other assets keep their place relative to the pages."""
    svg = "<svg xmlns='http://www.w3.org/2000/svg'/>"
    build = build_site(
        tmp_path,
        {
            "src/Foo/docs/sdd.md": sdd(body="![diagram](img/diagram.svg)"),
            "src/Foo/docs/img/diagram.svg": svg,
            "src/Foo/docs/Foo.svg": svg,
            "src/Foo/docs/.DS_Store": "",
        },
    )
    assert build.returncode == 0, build.log
    assert (build.site / "components/Foo/img/diagram.svg").read_text() == svg
    assert (build.site / "components/Foo/Foo.svg").is_file()
    assert not (build.site / "components/Foo/.DS_Store").exists()
    assert 'src="img/diagram.svg"' in build.html("components/Foo/index.html")


def test_edit_links_point_into_the_source_repository(tmp_path):
    """Edit links use the source edit_uri; hand-written pages use the site's."""
    build = build_site(
        tmp_path,
        {"src/Drv/Foo/docs/sdd.md": sdd(), "src/Drv/Foo/docs/extra.md": "# Extra\n"},
    )
    assert build.returncode == 0, build.log
    edit = "https://example.com/repo/edit/main/src/Drv/Foo/docs/"
    assert f'href="{edit}sdd.md"' in build.html("components/Foo/index.html")
    assert f'href="{edit}extra.md"' in build.html("components/Foo/extra/index.html")
    home = build.html("index.html")
    assert 'href="https://example.com/repo/edit/main/docs-site/index.md"' in home


def test_source_paths_are_watched_by_serve(tmp_path):
    """on_config adds each source path to config.watch for live reload."""
    build_site(tmp_path, {"src/Foo/docs/sdd.md": sdd()})
    config = load_config(str(tmp_path / "mkdocs.yml"))
    config = config.plugins.on_config(config)
    assert str(tmp_path / "src") in config.watch


def _source(root: Path) -> "component_docs.Source":
    """A Source rooted at `root`."""
    return component_docs.Source(path=root, edit_uri="https://e/")


def _touch(root: Path, *paths: str):
    """Create small Markdown files at `paths` under `root`."""
    for rel in paths:
        path = root / rel
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text("---\ntitle: X\n---\n# X\n")


def test_discovery_names_components_by_folder_and_skips_docs_and_dot_folders(
    tmp_path,
):
    _touch(
        tmp_path,
        "Components/Drv/Ina219Manager/docs/sdd.md",
        "Components/Drv/Ina219Manager/docs/sub/docs/sdd.md",
        "Components/.cache/Hidden/docs/sdd.md",
        "Top/docs/sdd.md",
    )
    components, errors = component_docs.discover(_source(tmp_path), tmp_path)
    assert errors == []
    assert sorted(c.name for c in components) == ["Ina219Manager", "Top"]
    ina = next(c for c in components if c.name == "Ina219Manager")
    assert ina.pages == (
        component_docs.Page(
            tmp_path / "Components/Drv/Ina219Manager/docs/sub/docs/sdd.md", "X"
        ),
    )


def test_menu_orders_categories_then_titles_with_other_last(tmp_path):
    """Categories keep config order, titles sort, Other comes last."""

    def component(name, title, category, pages=()):
        """A Component with an sdd.md and optional (file, title) extra pages."""
        docs = tmp_path / name / "docs"
        return component_docs.Component(
            name,
            _source(tmp_path),
            docs,
            component_docs.Page(docs / "sdd.md", title),
            category,
            tuple(component_docs.Page(docs / f, t) for f, t in pages),
            (),
        )

    registry = {
        c.name: c
        for c in [
            component("Zed", "Zed", "Typo"),
            component("Beta", "beta", "Core"),
            component("Alpha", "Alpha", "Driver"),
            component("Mode", "Mode", "Core", [("z.md", "A page"), ("a.md", "Z page")]),
        ]
    }
    menu, notes = component_docs.build_menu(registry, ["Core", "Driver", "Unused"])
    assert menu == [
        {
            "Core": [
                {"beta": "components/Beta/sdd.md"},
                {
                    "Mode": [
                        "components/Mode/sdd.md",
                        {"A page": "components/Mode/z.md"},
                        {"Z page": "components/Mode/a.md"},
                    ]
                },
            ]
        },
        {"Driver": [{"Alpha": "components/Alpha/sdd.md"}]},
        {"Other": [{"Zed": "components/Zed/sdd.md"}]},
    ]
    assert notes == [
        "Zed: category 'Typo' is not one of Core, Driver, Unused; listing it under Other"
    ]


def test_page_title_falls_back_to_default_then_heading_then_file_name(tmp_path):
    """Front matter wins; an SDD falls back to its component name, a page to its H1."""
    files = {
        "titled.md": "---\ntitle: Front\n---\n# Heading\n",
        "headed.md": "# Heading ##\n",
        "bare.md": "text\n",
    }
    for name, text in files.items():
        (tmp_path / name).write_text(text)

    def title(name, default=None):
        """The title read_page gives the file `name`."""
        return component_docs.read_page(tmp_path / name, default)[0].title

    assert title("titled.md", "Foo") == "Front"
    assert title("headed.md", "Foo") == "Foo"
    assert title("headed.md") == "Heading"
    assert title("bare.md") == "bare"


def test_submodule_sources_lists_only_sources_that_are_submodules(tmp_path):
    """The docs-deps CLI prints declared sources found in .gitmodules, nothing else."""
    lib = {"path": "lib/extras/", "edit_uri": "https://example.com/lib/edit/main/"}
    config = {"extra": {"component_sources": [SOURCE, lib, {"path": "lib/fprime"}]}}
    (tmp_path / "mkdocs.yml").write_text(yaml.safe_dump(config))
    (tmp_path / ".gitmodules").write_text(
        '[submodule "lib/extras"]\n\tpath = lib/extras\n\turl = https://e/x.git\n'
        '[submodule "lib/fprime"]\n\tpath = lib/fprime\n\turl = https://e/f.git\n'
    )
    result = subprocess.run(
        [
            sys.executable,
            str(HOOK),
            "--submodule-sources",
            str(tmp_path / "mkdocs.yml"),
        ],
        capture_output=True,
        text=True,
    )
    assert result.returncode == 0, result.stderr
    assert result.stdout.split() == ["lib/extras"]


def test_fill_menu_warns_when_there_is_no_entry_to_fill(caplog):
    """No nav, or a nav without the entry, warns and leaves nav as it was."""
    menu = [{"Core": [{"Foo": "components/Foo/sdd.md"}]}]
    nav = [{"Home": "index.md"}, {"Component SDDs": []}, {"More": "more.md"}]
    assert component_docs.fill_menu(nav, menu) == [
        {"Home": "index.md"},
        {"Component SDDs": menu},
        {"More": "more.md"},
    ]
    assert caplog.records == []
    assert component_docs.fill_menu(None, menu) is None
    assert component_docs.fill_menu([{"Home": "index.md"}], menu) == [
        {"Home": "index.md"}
    ]
    warnings = [r for r in caplog.records if r.levelname == "WARNING"]
    assert [r.getMessage() for r in warnings] == [
        "mkdocs.yml nav has no 'Component SDDs' entry for the hook to fill"
    ] * 2

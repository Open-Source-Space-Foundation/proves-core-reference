# Research #574: mkdocs-gen-files or MkDocs `hooks:` for component docs outside `docs-site/`

Ticket: [#574](https://github.com/Open-Source-Space-Foundation/proves-core-reference/issues/574), part of map [#573](https://github.com/Open-Source-Space-Foundation/proves-core-reference/issues/573).
Date: 2026-10-15. This branch (`research/docs-generation-tooling`) is never merged.

## Answer

Use MkDocs' built-in `hooks:` option: one local Python file with an `on_files` handler that adds every file in each component `docs/` folder as a virtual file through `File.generated(config, "components/<Name>/<path>", abs_src_path=<source>)`.

Both mechanisms passed every criterion in the ticket, and the two prototypes built the same site (the only differences were the order of entries in `search_index.json` and `sitemap.xml`). They differ in cost, not capability:

- **Hooks** add no dependency. They read the source files where they are, and `make docs-serve`, `deploy-docs.yml` and `requirements-docs.txt` need no new package. The costs are a floor bump to `mkdocs>=1.6` (for `File.generated`) and three extra lines if a page's edit link must be an absolute URL, as it must for a git submodule.
- **mkdocs-gen-files 0.6.1** handles absolute edit URLs natively and works back to MkDocs 1.4.1. It is a new dependency, though. It now pulls in `properdocs` (a MkDocs 1.x fork), caps `mkdocs<=1.6.1`, and prints a "switch to ProperDocs" banner on every `mkdocs` run. It also copies every file into a temporary directory on each build, and `make docs-serve` must gain `--with mkdocs-gen-files`.

## Method

I built two throwaway prototypes in `research/574-prototype/`, one per mechanism. Both use the same discovery function and the real `mkdocs.yml` theme, extensions and nav, with nav paths rewritten to `components/<Name>/sdd.md`:

| File | Purpose |
|---|---|
| `discover.py` | Finds every `**/docs/sdd.md` under each source root and yields every file in that `docs/` folder as `components/<ComponentName>/<path inside docs/>`. Fails if a root has no `sdd.md`. |
| `hooks.py` | Prototype B: `on_files` plus `on_page_markdown`. |
| `gen_components.py` | Prototype A: a mkdocs-gen-files script. |
| `make_configs.py` | Writes `mkdocs-hooks.yml` and `mkdocs-genfiles.yml` from the real `mkdocs.yml`. |
| `check_site.py` | Reads a built site and prints the assets, image `src` values, edit links and Mermaid block count. |

Environment: Python 3.13; mkdocs 1.6.1, mkdocs-material 9.7.7, pymdown-extensions 12.3, mkdocs-gen-files 0.6.1, properdocs 1.6.7. This is what `requirements-docs.txt` resolves to today (`uv pip compile`).

`docs_dir` pointed at `/tmp/mkres/docs-site-nocomponents`. That directory is a copy of `docs-site/` with `components/` deleted and the `index.md` links rewritten to `components/<Name>/sdd.md`. It simulates the cleanup the spec will do.

To reproduce:

```sh
cp -R docs-site /tmp/mkres/docs-site-nocomponents && rm -rf /tmp/mkres/docs-site-nocomponents/components
sed -i '' -E 's#\(components/([A-Za-z0-9]+)\.md#(components/\1/sdd.md#g' /tmp/mkres/docs-site-nocomponents/index.md
python3 research/574-prototype/make_configs.py
uvx --from mkdocs-material mkdocs build -f research/574-prototype/mkdocs-hooks.yml -d /tmp/mkres/site-hooks
uvx --from mkdocs-material --with mkdocs-gen-files mkdocs build -f research/574-prototype/mkdocs-genfiles.yml -d /tmp/mkres/site-genfiles
python3 research/574-prototype/check_site.py /tmp/mkres/site-hooks
```

**Baseline:** `mkdocs build --strict` on `main` already fails with 6 warnings. All 6 are links to `additional-resources/board-list.md` and `specifying-board-configuration.md`, which don't exist. Both prototypes give those same 6 warnings, plus 2 new ones (see [Gotchas](#gotchas)).

## Per-criterion comparison

| Criterion | mkdocs-gen-files 0.6.1 | MkDocs `hooks:` (`on_files`) |
|---|---|---|
| Non-Markdown assets copied | Yes. Each file is written with `mkdocs_gen_files.open(path, "wb")`. Verified: `components/Watchdog/img/diagram.svg` and `components/LoadSwitch/LoadSwitch.svg` reach the site. | Yes. Any `File.generated(..., abs_src_path=...)` that isn't a page is copied as a media file. Same two SVGs verified. |
| Relative image links resolve | Yes, because the `docs/` folder structure is kept under `components/<Name>/`. `![...](img/diagram.svg)` renders as `src="../img/diagram.svg"` from `components/Watchdog/sdd/`. No link warnings from any `sdd.md`. | Same result, byte for byte. |
| `mkdocs serve` live reload | Only with `watch:`. With `watch: [PROVESFlightControllerReference]`, appending a marker to `Watchdog/docs/sdd.md` showed up on the served page about 6 s later. Without `watch:`, the page stayed stale. Each rebuild reruns the script, because `serve` reloads the config and runs a full build (`mkdocs/commands/serve.py`). | Same results with and without `watch:`. `mkdocs serve` watches only `docs_dir`, the config file and the `watch:` entries (`serve.py` lines 88-101). The hook module is re-executed on each rebuild because the config is reloaded. |
| "Edit this page" points at the source | Yes. `set_edit_path(page, "../PROVESFlightControllerReference/.../docs/sdd.md")` with `edit_uri: edit/main/docs-site/` gave `.../edit/main/PROVESFlightControllerReference/Components/Watchdog/docs/sdd.md`. The plugin `urljoin`s the path onto `repo_url + edit_uri` in `on_page_content` (`mkdocs_gen_files/plugin.py`), so an absolute URL also works unchanged. Call it for `.md` files only: for assets it logs a "set_edit_path invocations went unused" warning, which fails `--strict`. | Yes. Set `File.edit_uri = "../PROVESFlightControllerReference/..."`, which MkDocs appends to `edit_uri` (`Page._set_edit_url`, `mkdocs/structure/pages.py`). Same URL verified. An absolute URL can't go through `File.edit_uri`, because MkDocs builds `edit_uri + file.edit_uri`. Set `page.edit_url` in `on_page_markdown` instead (verified, 4 lines in `hooks.py`). |
| Mermaid still renders | Yes. 36 component pages, 18 of them with `<pre class="mermaid">`, which matches the 18 sources that use Mermaid fences. The generated files go through the same Markdown pipeline as any other page. | Same: 36 pages, 18 with Mermaid. |
| Git-submodule source folder | Works. Tested with a separate git repo as a second source root and an absolute edit base: its page, its `img/` asset and its Mermaid block all built, and the edit link pointed at that repo. | Works. Same test and same result. |
| `requirements-docs.txt` compatibility | Resolves with the current file to mkdocs 1.6.1, material 9.7.7, gen-files 0.6.1 and properdocs 1.6.7. It needs a new requirements line and `--with mkdocs-gen-files` in `make docs-serve`. Without that flag, `uvx --from mkdocs-material` fails with `The "gen-files" plugin is not installed`. | No new package. It needs `mkdocs>=1.6` in place of `>=1.5.0`: on mkdocs 1.5.3 with material 9.5.18 (a combination the current file allows), the hook fails with `AttributeError: type object 'File' has no attribute 'generated'`. Material 9.7.x already requires `mkdocs>=1.6,<2`, so this only records what pip already installs. `make docs-serve` works unchanged. |

## Dependency and ecosystem context

- mkdocs-gen-files 0.6.1 (2026-03-16) declares `Requires-Dist: mkdocs<=1.6.1,>=1.4.1` and `Requires-Dist: properdocs>=1.6.5` (from its wheel `METADATA`). Its `plugin.py` calls `properdocs.replacement_warning.setup()`, which prints a multi-line banner telling users to switch from `mkdocs` to `properdocs`, unless `DISABLE_MKDOCS_2_WARNING=true` is set. ProperDocs is a community continuation of MkDocs 1.x ([properdocs/discussions/33](https://github.com/ProperDocs/properdocs/discussions/33)).
- mkdocs-material 9.7.x already prints its own banner about MkDocs 2.0, which it says drops plugins, theme overrides and migration ([Material blog, 2026-02-18](https://squidfunk.github.io/mkdocs-material/blog/2026/02/18/mkdocs-2.0/)). PyPI's latest stable mkdocs is still 1.6.1 (2024-08-30), with `2.0.dev*` pre-releases. Both mechanisms depend on the MkDocs 1.x plugin API, so a future move to MkDocs 2 or another tool breaks them equally. The hook is about 20 lines to port. The plugin depends on its maintainer.
- `hooks:` was added in MkDocs 1.4 ([configuration docs](https://www.mkdocs.org/user-guide/configuration/#hooks)). `File.generated` and in-memory or `abs_src_path` virtual files were added in 1.6 (docstring in `mkdocs/structure/files.py`).

## Gotchas

These apply to either mechanism unless noted.

1. **`exclude_docs` cannot hide the old copies during migration.** MkDocs calls `set_exclusions` again after `on_files` (`mkdocs/commands/build.py`, right after `config.plugins.on_files`). A pattern such as `/components/` therefore excludes the generated files too: all 34 nav entries reported "excluded from the built site". Delete `docs-site/components/` in the same change.
2. **The site layout must keep each `docs/` folder intact.** Relative links such as `img/diagram.svg`, `LoadSwitch.svg` and `./sdd.md` resolve only because `components/<Name>/` mirrors `<Component>/docs/`. If `sdd.md` became `components/<Name>.md` or `components/<Name>/index.md`, the image links or the sibling `./sdd.md` links would break. Page URLs change from `components/Watchdog/` to `components/Watchdog/sdd/`, so `nav` and the 39 `components/*.md` links in `docs-site/index.md` must be rewritten.
3. **Links that leave the `docs/` folder break.** `ModeManager/docs/safe-mode-fsm.md` links to `../../../docs-site/components/ModeManager.md` and `../../../test/int/safe_mode_test.py`. Both now produce warnings (2 new `--strict` failures). Point the first at `sdd.md` and the second at an absolute GitHub URL. A convention for the future library repo: component docs link only within their own `docs/` folder or to absolute URLs.
4. **The edit button needs config the site doesn't have today.** With `repo_url` set and no `edit_uri`, MkDocs defaults to `edit/master/docs/` (`mkdocs/config/config_options.py`), which is wrong here on two counts. Material shows the button only with the `content.action.edit` theme feature ([Material: code actions](https://squidfunk.github.io/mkdocs-material/setup/adding-a-git-repository/#code-actions)). The prototypes set `edit_uri: edit/main/docs-site/` and enable that feature.
5. **`watch:` is required, and each source root is a separate entry.** Add `PROVESFlightControllerReference`, and later the submodule path. That directory is 1.8 MB and 276 files, so watching it is cheap. Files that the hook imports are cached in `sys.modules` across `serve` rebuilds, so keep the hook to one file or add it to `watch:` as well. The hook file itself is re-executed on each rebuild.
6. **A submodule that isn't checked out is an empty directory.** The discovery would find nothing and the build would still succeed. In this worktree, `lib/fprime-extras` is exactly that. The prototype's `discover.py` exits with an error when a root has no `sdd.md`. Verified: the hook build aborts with that message, and gen-files turns it into `PluginError` / `Aborted with a BuildError!`.
7. **The deploy workflow doesn't fetch submodules.** `.github/workflows/deploy-docs.yml` uses `actions/checkout@v6` without `submodules:`, so a submodule source root would be empty in CI. Add `submodules: true`, or check out just that path.
8. **A submodule's edit link must point at its own repository.** On GitHub, a submodule path in the parent repo is a gitlink, so `edit/main/<submodule path>/...` returns 404. Each source root needs its own edit base, such as `https://github.com/<org>/<library>/edit/main`. Choose a branch, not the pinned SHA, because GitHub's editor needs a branch. Each source-folder entry should therefore carry `{path, edit_base}`.
9. **Duplicate component names collide silently.** Pages are keyed by `components/<docs parent dir name>`. Two source roots with the same component name overwrite each other. With hooks, `Files.append` deletes the earlier entry and emits only a `DeprecationWarning`, which isn't shown by default (`mkdocs/structure/files.py`). With gen-files, the second `open(..., "wb")` overwrites the first. The discovery should reject duplicates.
10. **Non-`sdd` pages aren't in `nav`.** `PayloadCom/docs/USAGE.md` and `BUFFER_USAGE.md` build and are reachable, but MkDocs logs them at INFO level as "not included in the nav". That's an INFO, not a `--strict` failure. Whether they get nav entries is a spec decision.

## Sources

Installed package source (Python 3.13 venv, versions above):

- `mkdocs/commands/build.py`: `on_files`, then `set_exclusions`.
- `mkdocs/commands/serve.py`: `get_config()` on each rebuild; the watch list is `docs_dir`, the config file and `config.watch`.
- `mkdocs/structure/files.py`: `File.generated`, `File.edit_uri` ("For generated files this should be set to `None`"), `Files.append`, `set_exclusions`.
- `mkdocs/structure/pages.py`: `Page._set_edit_url`.
- `mkdocs/config/config_options.py`: the `edit_uri` default and the `Hooks` loader (adds the hook's directory to `sys.path` only during import).
- `mkdocs_gen_files/plugin.py`, `mkdocs_gen_files/editor.py`: temp-dir `FilesEditor`, `set_edit_path`, the edit URL `urljoin`, the "went unused" warning.
- `mkdocs_gen_files-0.6.1.dist-info/METADATA`, `mkdocs_material-9.7.7.dist-info/METADATA`: dependency pins.
- `properdocs/replacement_warning.py`: banner text and `DISABLE_MKDOCS_2_WARNING`.

Docs:

- MkDocs configuration: [hooks](https://www.mkdocs.org/user-guide/configuration/#hooks), [watch](https://www.mkdocs.org/user-guide/configuration/#watch), [edit_uri](https://www.mkdocs.org/user-guide/configuration/#edit_uri), [exclude_docs](https://www.mkdocs.org/user-guide/configuration/#exclude_docs).
- [mkdocs-gen-files docs](https://oprypin.github.io/mkdocs-gen-files/) and [source](https://github.com/oprypin/mkdocs-gen-files).
- [Material: adding a git repository / code actions](https://squidfunk.github.io/mkdocs-material/setup/adding-a-git-repository/), [Material: diagrams (Mermaid)](https://squidfunk.github.io/mkdocs-material/reference/diagrams/).
- PyPI JSON API for release dates: `mkdocs`, `mkdocs-gen-files`, `mkdocs-material`, `properdocs`.

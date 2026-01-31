# Documentation Site

This directory contains the MkDocs documentation site for PROVES Core Reference Software Design Documents (SDDs).

## Structure

- `docs-site/` - Source documentation files
  - `index.md` - Home page
  - `components/` - Individual component SDD files
    - `*.md` - Component documentation (copied from original locations)
    - `img/` - Images used in documentation

## Building the Documentation

To build the documentation locally:

```bash
# Using Make (recommended)
make docs-serve  # Start local development server at http://127.0.0.1:8000
make docs-build  # Build static site to ./site directory

# Or using pip and mkdocs directly
pip install -r requirements-docs.txt
mkdocs serve     # Start development server
mkdocs build     # Build static site
```

The built site will be in the `site/` directory.

## Deployment

The documentation is automatically deployed to GitHub Pages when changes are pushed to the main branch via the `.github/workflows/deploy-docs.yml` workflow.

Once deployed, the site will be available at:
https://open-source-space-foundation.github.io/proves-core-reference/

## Updating Documentation

The SDD files are copied from their original locations in `FprimeZephyrReference/` to `docs-site/components/`.

To update the documentation after making changes to component SDDs:

```bash
# Sync all SDD files from components to docs-site
make docs-sync
```

This will:
- Copy all 31 component SDD files from `FprimeZephyrReference/` to `docs-site/components/`
- Copy all images from component `docs/img/` directories to `docs-site/components/img/`

**Note**: After syncing, you may need to fix any broken links or formatting issues in the copied files before committing. See the commit history for examples of common fixes needed (e.g., updating F Prime documentation links to point to online resources).

Manual update process:
1. Edit the original SDD files in their component directories
2. Run `make docs-sync` to copy changes to `docs-site/components/`
3. Review and fix any broken links or formatting issues
4. Commit and push changes

## Configuration

The MkDocs configuration is in `mkdocs.yml` at the repository root.

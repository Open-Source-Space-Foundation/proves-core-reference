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
# Install dependencies
pip install -r requirements-docs.txt

# Build the site
mkdocs build

# Serve the site locally for preview
mkdocs serve
```

The built site will be in the `site/` directory.

## Deployment

The documentation is automatically deployed to GitHub Pages when changes are pushed to the main branch via the `.github/workflows/deploy-docs.yml` workflow.

Once deployed, the site will be available at:
https://open-source-space-foundation.github.io/proves-core-reference/

## Updating Documentation

The SDD files are copied from their original locations in `FprimeZephyrReference/` to `docs-site/components/`. 

To update the documentation:

1. Edit the original SDD files in their component directories
2. Run the copy script to sync changes to `docs-site/components/`
3. Commit and push changes

## Configuration

The MkDocs configuration is in `mkdocs.yml` at the repository root.

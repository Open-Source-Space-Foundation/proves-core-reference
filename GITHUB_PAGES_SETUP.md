# Setting Up GitHub Pages Deployment

After merging this PR, you need to enable GitHub Pages in the repository settings:

## Steps to Enable GitHub Pages

1. Go to the repository settings: https://github.com/Open-Source-Space-Foundation/proves-core-reference/settings
2. Navigate to "Pages" in the left sidebar
3. Under "Build and deployment":
   - **Source**: Select "GitHub Actions"
4. Save the settings

## First Deployment

Once the settings are configured and this PR is merged to main:

1. The workflow will automatically run on the next push to main
2. The documentation will be deployed to: https://open-source-space-foundation.github.io/proves-core-reference/
3. Future updates will automatically redeploy when changes are pushed to main

## Manual Deployment

You can also trigger a manual deployment:

1. Go to Actions tab: https://github.com/Open-Source-Space-Foundation/proves-core-reference/actions
2. Select "Deploy MkDocs to GitHub Pages" workflow
3. Click "Run workflow" button
4. Select the main branch and run

## Troubleshooting

If the deployment fails:

1. Check the Actions tab for error logs
2. Ensure GitHub Pages is enabled and set to "GitHub Actions" source
3. Verify the workflow has the necessary permissions (set in the workflow file)
4. Check that Python dependencies are correctly specified in requirements-docs.txt

## Local Development

To preview the documentation locally:

```bash
# Using Make (recommended)
make docs-serve

# Or using pip and mkdocs directly
pip install -r requirements-docs.txt
mkdocs serve

# Visit http://127.0.0.1:8000 in your browser
```

To build the static site:

```bash
# Using Make
make docs-build

# Or using mkdocs directly
mkdocs build
```

The built site will be in the `site/` directory.

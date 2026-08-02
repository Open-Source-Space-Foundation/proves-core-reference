---
name: debug-ci
description: Debug CI build failures for this project. Use when a GitHub Actions run is failing, tests are broken, linting is failing, or the user wants to investigate a CI issue on the current branch or a specific PR/run.
version: 1.0.0
---

# Debug CI Build Issues

This project uses GitHub Actions with these jobs:
- **lint** — runs `make fmt` (pre-commit hooks: clang-format, cpplint, ruff, codespell, trailing whitespace, etc.)
- **unit-test** — runs `make test-unit` (gtest C++ tests in `PROVESFlightControllerReference/test/unit-tests/`)
- **build** — compiles Zephyr firmware on the `deathstar` self-hosted runner
- **integration-radio** — runs pytest integration tests on the `integration` self-hosted runner (requires physical hardware — cannot be run locally)

Integration tests live in `PROVESFlightControllerReference/test/int/`.
Unit tests live in `PROVESFlightControllerReference/test/unit-tests/`.
Linting config: `.pre-commit-config.yaml`.

## Workflow

### 1. Find the failing run

If no run ID or PR number was given, find the most recent run on the current branch:

```bash
gh run list --branch $(git branch --show-current) --limit 5
```

If a PR number was given:
```bash
gh pr checks <PR_NUMBER>
```

### 2. Get the failing job and step

```bash
gh run view <RUN_ID> --log-failed
```

Or for a specific job:
```bash
gh run view <RUN_ID> --job <JOB_ID> --log
```

If the run is still in progress:
```bash
gh run watch <RUN_ID>
```

### 3. Diagnose by failure type

#### Lint failure (`make fmt`)

The lint job runs `make fmt`. Failures mean files need formatting or have linting errors.

Common causes and fixes:
- **clang-format**: C++ file needs reformatting — run `clang-format -i <file>` or `make fmt` locally
- **cpplint**: C++ style violation — check `cpplint.cfg` for configured filters, fix the flagged lines
- **ruff-check**: Python linting error — run `ruff check --fix <file>`
- **ruff-format**: Python formatting — run `ruff format <file>`
- **codespell**: Typo in a comment or string — add to `.codespell-ignore-words.txt` if intentional, otherwise fix
- **trailing-whitespace / end-of-file-fixer**: Whitespace issue — usually auto-fixed by running pre-commit

To reproduce locally:
```bash
make fmt
# or run pre-commit directly:
pre-commit run --all-files
```

#### Unit test failure (`make test-unit`)

Look for the specific test name and failure message in the log. Tests are in `PROVESFlightControllerReference/test/unit-tests/`.

Read the gtest output carefully:
- `[ FAILED ]` lines identify the specific test case
- The failure message shows the assertion that failed and file:line

To investigate: read the failing test file and the source it's testing.

#### Build failure (`make build`)

The build runs on `deathstar` (self-hosted). Build failures are usually:
- Compile errors from a C++ change — read the compiler output for file:line
- Missing generated files — `make generate` may need to run first
- Zephyr/fprime API mismatch — check recent submodule changes with `git diff HEAD~1 lib/`

Check submodule state if the error involves fprime or Zephyr headers:
```bash
git submodule status
```

#### Integration test failure (`make test-integration`)

Integration tests run on the `integration` self-hosted runner with physical hardware attached. **These cannot be reproduced locally without the hardware.**

The test runner uses these make targets:
- `make test-integration` — runs all integration tests
- `make test-integration FILTER=<test_name>` — runs a single test
- `make test-integration PYTEST_ARGS=--with-radio` — radio-specific tests

When reading pytest failures:
- Look for `FAILED` lines with the test function name
- Check the assertion error and any captured stdout/logs
- The GDS (Ground Data System) starts before tests run — if it crashes, look for "GDS process exited unexpectedly" in the log
- TTY detection failures mean a hardware device wasn't found/connected

For integration failures, examine the test source in `PROVESFlightControllerReference/test/int/` to understand what the test expects, then look at recent code changes that could affect that behavior.

### 4. Cross-reference with recent commits

After identifying the failure, check what changed:
```bash
gh run view <RUN_ID>  # shows the triggering commit SHA
git log --oneline -10
git diff <SHA>~1 <SHA> -- <relevant_path>
```

### 5. Re-run a failed job

If the failure looks flaky (hardware glitch, network timeout):
```bash
gh run rerun <RUN_ID> --failed
```

To rerun only a specific job:
```bash
gh run rerun <RUN_ID> --job <JOB_ID>
```

## Quick reference commands

```bash
# List recent runs on current branch
gh run list --branch $(git branch --show-current) --limit 10

# View failed logs for a run
gh run view <RUN_ID> --log-failed

# View full log for a specific job
gh run view <RUN_ID> --job <JOB_ID> --log

# Check PR CI status
gh pr checks <PR_NUMBER>

# Watch a running workflow
gh run watch <RUN_ID>

# Rerun failed jobs
gh run rerun <RUN_ID> --failed

# View workflow run details
gh run view <RUN_ID>
```

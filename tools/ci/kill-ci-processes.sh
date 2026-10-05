#!/usr/bin/env bash
# Kill the processes this checkout started (GDS, pytest, YAMCS adapter, uv
# wrappers): anything whose command line runs out of this checkout's
# fprime-venv or bin/ directory. Replaces a blanket `pkill -9 python`, which
# also killed unrelated Python processes on a runner host shared with other
# work (e.g. a bench machine running its own GDS against a second board).
set -u

repo_root=$(cd "$(dirname "$0")/../.." && pwd)

pkill -9 -f "$repo_root/fprime-venv/" || true
pkill -9 -f "$repo_root/bin/" || true

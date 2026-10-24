#!/bin/bash
# SITL smoke run on a fresh flash image. Checks BootCount on three boots: a first
# boot, a boot after a power cut (kill -9), and a boot after WARM_RESET.
# Usage: sim/smoke.sh <run-dir>
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
RUN="$(realpath -m "$1")"
cd "$ROOT"
# Delete only an empty directory or one that run-sil.sh wrote.
if [ -n "$(ls -A "$RUN" 2> /dev/null)" ] && [ ! -f "$RUN/sil.log" ]; then
    echo "$RUN is not a SITL run directory, refusing to delete it" >&2
    exit 1
fi
rm -rf "$RUN"
trap 'sim/stop-sil.sh "$RUN"' EXIT

expect_boot() {
    echo "== boot $1"
    make test-integration TEST=boot_count_test PYTEST_ARGS="--expect-boot-count=$1"
}

sim/run-sil.sh "$RUN"
expect_boot 1

kill -9 "$(cat "$RUN/sil.pid")"
sim/stop-sil.sh "$RUN"
sim/run-sil.sh "$RUN"
expect_boot 2

make test-integration TEST=reset_manager_test PYTEST_ARGS="-k warm_reset"
expect_boot 3
echo "SITL smoke passed, logs in $RUN"

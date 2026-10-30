#!/bin/bash
# SITL smoke run on a fresh flash image. Checks BootCount on three boots: a first
# boot, a boot after a power cut (kill -9), and a boot after WARM_RESET.
# Usage: sim/smoke.sh <run-dir>
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
RUN="$(realpath -m "$1")"
cd "$ROOT"
# Stop any run still using this directory, then remove only the files run-sil.sh and
# the sim write, so a mistyped path loses nothing else.
sim/stop-sil.sh "$RUN"
rm -f "$RUN"/{pcr-flash.bin,pcr-native_sim.exe,fprime.pty,sil.log,gds.log,sequence_number.bin} \
    "$RUN"/{die_temp,lsm6dso,lis2mdl,ina219_sys,ina219_sol}.txt
trap 'sim/stop-sil.sh "$RUN"' EXIT

expect_boot() {
    echo "== boot $1"
    make sim-test SIM_RUN="$RUN" TEST=boot_count_test PYTEST_ARGS="--expect-boot-count=$1"
}

sim/run-sil.sh "$RUN"
expect_boot 1

kill -9 "$(cat "$RUN/sil.pid")"
sim/stop-sil.sh "$RUN"
sim/run-sil.sh "$RUN"
expect_boot 2

make sim-test SIM_RUN="$RUN" TEST=reset_manager_test PYTEST_ARGS="-k warm_reset"
expect_boot 3
echo "SITL smoke passed, logs in $RUN"

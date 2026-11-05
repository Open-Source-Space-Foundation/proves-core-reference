#!/bin/bash
# Boot the native_sim build and attach a headless fprime-gds to its F' UART pty.
# Usage: [SIM_ARGS=-rt-ratio=10] sim/run-sil.sh <run-dir> [stop-at-seconds]
# Returns once the GDS is up. Stop both with sim/stop-sil.sh <run-dir>.
# <run-dir>/pcr-flash.bin persists across runs.
set -euo pipefail
# The sim binary embeds the authentication key, so keep the run directory's files private.
umask 077
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
mkdir -p "$1"
RUN="$(cd "$1" && pwd)"
STOP_AT="${2:-3600}"

# The sim's GDS keeps its uplink sequence number in the run directory, so it never
# moves the board's Framing/src/sequence_number.bin. `make sim-test` reads the same file.
export PROVES_SEQUENCE_NUMBER_FILE="$RUN/sequence_number.bin"
# A fresh flash image starts the flight software's uplink sequence number at 0.
[ -f "$RUN/pcr-flash.bin" ] || echo 0 > "$PROVES_SEQUENCE_NUMBER_FILE"

# sys_reboot() re-execs this path, so run a copy that a rebuild cannot replace mid-run.
cp "$ROOT/build-fprime-native_sim/zephyr/zephyr.exe" "$RUN/pcr-native_sim.exe"
# Every boot points fprime.pty at its new F' pty, so the GDS follows the sim across reboots.
rm -f "$RUN/fprime.pty"
# Each boot appends to sil.log and gds.log.
echo "== run-sil.sh $(date -Is)" >> "$RUN/sil.log"
echo "== run-sil.sh $(date -Is)" >> "$RUN/gds.log"
read -ra SIM_ARGV <<< "${SIM_ARGS:-}"
(cd "$RUN" && exec ./pcr-native_sim.exe -flash=pcr-flash.bin -stop_at="$STOP_AT" \
    -uart_2_attach_uart_cmd="ln -sfn %s fprime.pty" "${SIM_ARGV[@]}" >> sil.log 2>&1) &
echo $! > "$RUN/sil.pid"
# On a failed start, stop what this run started.
trap '"$ROOT/sim/stop-sil.sh" "$RUN"' EXIT

for _ in $(seq 50); do
    [ -L "$RUN/fprime.pty" ] && break
    sleep 0.1
done
[ -L "$RUN/fprime.pty" ] || { echo "sim did not open its F' pty, see $RUN/sil.log" >&2; exit 1; }

# fprime-gds reads fprime-gds.yml from the repo root. setsid gives the GDS and its
# children one process group, so stop-sil.sh can stop them together. Unbuffered
# output lets the readiness check below see "F prime is now running" in gds.log.
cd "$ROOT"
GDS_LOG_START=$(($(wc -l < "$RUN/gds.log") + 1))
PYTHONUNBUFFERED=1 setsid "$ROOT/fprime-venv/bin/fprime-gds" --gui=none --uart-device "$RUN/fprime.pty" --uart-skip-port-check \
    --dictionary "$ROOT/build-artifacts-sim/zephyr/fprime-zephyr-deployment/dict/ReferenceDeploymentTopologyDictionary.json" \
    >> "$RUN/gds.log" 2>&1 &
echo $! > "$RUN/gds.pid"
gds_up() { awk -v start="$GDS_LOG_START" 'NR >= start && /F prime is now running/ { up = 1; exit } END { exit !up }' "$RUN/gds.log"; }
for _ in $(seq 600); do
    gds_up && break
    sleep 0.1
done
gds_up || { echo "GDS did not start, see $RUN/gds.log" >&2; exit 1; }
trap - EXIT
echo "sim pid $(cat "$RUN/sil.pid"), F' UART on $(readlink "$RUN/fprime.pty"), gds pid $(cat "$RUN/gds.pid")"
